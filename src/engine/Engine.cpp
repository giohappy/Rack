#include "engine/Engine.hpp"
#include "settings.hpp"
#include "system.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <omp.h>
#include <xmmintrin.h>
#include <pmmintrin.h>


namespace rack {


/** Threads which obtain a VIPLock will cause wait() to block for other less important threads.
This does not provide the VIPs with an exclusive lock. That should be left up to another mutex shared between the less important thread.
*/
struct VIPMutex {
	int count = 0;
	std::condition_variable cv;
	std::mutex countMutex;

	/** Blocks until there are no remaining VIPLocks */
	void wait() {
		std::unique_lock<std::mutex> lock(countMutex);
		while (count > 0)
			cv.wait(lock);
	}
};

struct VIPLock {
	VIPMutex &m;
	VIPLock(VIPMutex &m) : m(m) {
		std::unique_lock<std::mutex> lock(m.countMutex);
		m.count++;
	}
	~VIPLock() {
		std::unique_lock<std::mutex> lock(m.countMutex);
		m.count--;
		lock.unlock();
		m.cv.notify_all();
	}
};


struct Engine::Internal {
	bool running = false;
	float sampleRate;
	float sampleTime;
	float sampleRateRequested;

	int nextModuleId = 1;
	int nextCableId = 1;

	// Parameter smoothing
	Module *smoothModule = NULL;
	int smoothParamId;
	float smoothValue;

	std::mutex mutex;
	std::thread thread;
	VIPMutex vipMutex;
};


Engine::Engine() {
	internal = new Internal;

	float sampleRate = 44100.f;
	internal->sampleRate = sampleRate;
	internal->sampleTime = 1 / sampleRate;
	internal->sampleRateRequested = sampleRate;

	threadCount = 1;
}

Engine::~Engine() {
	// Make sure there are no cables or modules in the rack on destruction. This suggests that a module failed to remove itself before the RackWidget was destroyed.
	assert(cables.empty());
	assert(modules.empty());

	delete internal;
}

static void Engine_step(Engine *engine) {
	// Sample rate
	if (engine->internal->sampleRateRequested != engine->internal->sampleRate) {
		engine->internal->sampleRate = engine->internal->sampleRateRequested;
		engine->internal->sampleTime = 1 / engine->internal->sampleRate;
		for (Module *module : engine->modules) {
			module->onSampleRateChange();
		}
	}

	// Param smoothing
	{
		Module *smoothModule = engine->internal->smoothModule;
		int smoothParamId = engine->internal->smoothParamId;
		float smoothValue = engine->internal->smoothValue;
		if (smoothModule) {
			Param *param = &smoothModule->params[smoothParamId];
			float value = param->value;
			// decay rate is 1 graphics frame
			const float smoothLambda = 60.f;
			float newValue = value + (smoothValue - value) * smoothLambda * engine->internal->sampleTime;
			if (value == newValue || !(param->minValue <= newValue && newValue <= param->maxValue)) {
				// Snap to actual smooth value if the value doesn't change enough (due to the granularity of floats), or if newValue is out of bounds
				param->setValue(smoothValue);
				engine->internal->smoothModule = NULL;
			}
			else {
				param->value = newValue;
			}
		}
	}

	const float cpuLambda = engine->internal->sampleTime / 2.f;

	// Iterate modules
	int modulesLen = engine->modules.size();
	#pragma omp parallel for num_threads(engine->threadCount) schedule(guided, 1)
	for (int i = 0; i < modulesLen; i++) {
		Module *module = engine->modules[i];
		if (!module->bypass) {
			// Step module
			if (settings::powerMeter) {
				auto startTime = std::chrono::high_resolution_clock::now();

				module->step();

				auto stopTime = std::chrono::high_resolution_clock::now();
				float cpuTime = std::chrono::duration<float>(stopTime - startTime).count();
				// Smooth cpu time
				module->cpuTime += (cpuTime - module->cpuTime) * cpuLambda;
			}
			else {
				module->step();
			}
		}

		// Iterate ports and step plug lights
		for (Input &input : module->inputs) {
			input.step();
		}
		for (Output &output : module->outputs) {
			output.step();
		}
	}

	// Step cables
	for (Cable *cable : engine->cables) {
		cable->step();
	}
}

static void Engine_run(Engine *engine) {
#if defined ARCH_LIN
	// Name thread
	system::setThreadName("Engine");
#endif
	// Set CPU to flush-to-zero (FTZ) and denormals-are-zero (DAZ) mode
	// https://software.intel.com/en-us/node/682949
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	// Every time the engine waits and locks a mutex, it steps this many frames
	const int mutexSteps = 64;
	// Time in seconds that the engine is rushing ahead of the estimated clock time
	double ahead = 0.0;
	auto lastTime = std::chrono::high_resolution_clock::now();

	while (engine->internal->running) {
		engine->internal->vipMutex.wait();

		if (!engine->paused) {
			std::lock_guard<std::mutex> lock(engine->internal->mutex);
			// auto startTime = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < mutexSteps; i++) {
				Engine_step(engine);
			}

			// auto stopTime = std::chrono::high_resolution_clock::now();
			// float cpuTime = std::chrono::duration<float>(stopTime - startTime).count();
			// DEBUG("%g", cpuTime / mutexSteps * 44100);
		}

		double stepTime = mutexSteps * engine->internal->sampleTime;
		ahead += stepTime;
		auto currTime = std::chrono::high_resolution_clock::now();
		const double aheadFactor = 2.0;
		ahead -= aheadFactor * std::chrono::duration<double>(currTime - lastTime).count();
		lastTime = currTime;
		ahead = std::fmax(ahead, 0.0);

		// Avoid pegging the CPU at 100% when there are no "blocking" modules like AudioInterface, but still step audio at a reasonable rate
		// The number of steps to wait before possibly sleeping
		const double aheadMax = 1.0; // seconds
		if (ahead > aheadMax) {
			std::this_thread::sleep_for(std::chrono::duration<double>(stepTime));
		}
	}
}

void Engine::start() {
	internal->running = true;
	internal->thread = std::thread(Engine_run, this);
}

void Engine::stop() {
	internal->running = false;
	internal->thread.join();
}

void Engine::addModule(Module *module) {
	assert(module);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);
	// Check that the module is not already added
	auto it = std::find(modules.begin(), modules.end(), module);
	assert(it == modules.end());
	// Set ID
	if (module->id == 0) {
		// Automatically assign ID
		module->id = internal->nextModuleId++;
	}
	else {
		// Manual ID
		assert(module->id < internal->nextModuleId);
		// Check that the ID is not already taken
		for (Module *m : modules) {
			assert(module->id != m->id);
		}
	}
	// Add module
	modules.push_back(module);
}

void Engine::removeModule(Module *module) {
	assert(module);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);
	// If a param is being smoothed on this module, stop smoothing it immediately
	if (module == internal->smoothModule) {
		internal->smoothModule = NULL;
	}
	// Check that all cables are disconnected
	for (Cable *cable : cables) {
		assert(cable->outputModule != module);
		assert(cable->inputModule != module);
	}
	// Check that the module actually exists
	auto it = std::find(modules.begin(), modules.end(), module);
	assert(it != modules.end());
	// Remove the module
	modules.erase(it);
	// Remove id
	module->id = 0;
}

void Engine::resetModule(Module *module) {
	assert(module);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);

	module->reset();
}

void Engine::randomizeModule(Module *module) {
	assert(module);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);

	module->randomize();
}

void Engine::bypassModule(Module *module, bool bypass) {
	assert(module);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);
	if (bypass) {
		for (Output &output : module->outputs) {
			// This also zeros all voltages
			output.setChannels(0);
		}
		module->cpuTime = 0.f;
	}
	else {
		// Set all outputs to 1 channel
		for (Output &output : module->outputs) {
			// Don't use Port::setChannels() so we maintain all previous voltages
			output.channels = 1;
		}
	}
	module->bypass = bypass;
}

static void Engine_updateConnected(Engine *engine) {
	// Set everything to unconnected
	for (Module *module : engine->modules) {
		for (Input &input : module->inputs) {
			input.active = false;
		}
		for (Output &output : module->outputs) {
			output.active = false;
		}
	}
	// Set inputs/outputs to active
	for (Cable *cable : engine->cables) {
		cable->outputModule->outputs[cable->outputId].active = true;
		cable->inputModule->inputs[cable->inputId].active = true;
	}
}

void Engine::addCable(Cable *cable) {
	assert(cable);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);
	// Check cable properties
	assert(cable->outputModule);
	assert(cable->inputModule);
	// Check that the cable is not already added, and that the input is not already used by another cable
	for (Cable *cable2 : cables) {
		assert(cable2 != cable);
		assert(!(cable2->inputModule == cable->inputModule && cable2->inputId == cable->inputId));
	}
	// Set ID
	if (cable->id == 0) {
		// Automatically assign ID
		cable->id = internal->nextCableId++;
	}
	else {
		// Manual ID
		assert(cable->id < internal->nextCableId);
		// Check that the ID is not already taken
		for (Cable *w : cables) {
			assert(cable->id != w->id);
		}
	}
	// Add the cable
	cables.push_back(cable);
	Engine_updateConnected(this);
}

void Engine::removeCable(Cable *cable) {
	assert(cable);
	VIPLock vipLock(internal->vipMutex);
	std::lock_guard<std::mutex> lock(internal->mutex);
	// Check that the cable is already added
	auto it = std::find(cables.begin(), cables.end(), cable);
	assert(it != cables.end());
	// Set input to inactive
	Input &input = cable->inputModule->inputs[cable->inputId];
	input.setChannels(0);
	// Remove the cable
	cables.erase(it);
	Engine_updateConnected(this);
	// Remove ID
	cable->id = 0;
}

void Engine::setParam(Module *module, int paramId, float value) {
	// TODO Does this need to be thread-safe?
	module->params[paramId].value = value;
}

float Engine::getParam(Module *module, int paramId) {
	return module->params[paramId].value;
}

void Engine::setSmoothParam(Module *module, int paramId, float value) {
	// If another param is being smoothed, jump value
	if (internal->smoothModule && !(internal->smoothModule == module && internal->smoothParamId == paramId)) {
		internal->smoothModule->params[internal->smoothParamId].value = internal->smoothValue;
	}
	internal->smoothParamId = paramId;
	internal->smoothValue = value;
	internal->smoothModule = module;
}

float Engine::getSmoothParam(Module *module, int paramId) {
	if (internal->smoothModule == module && internal->smoothParamId == paramId)
		return internal->smoothValue;
	return getParam(module, paramId);
}

int Engine::getNextModuleId() {
	return internal->nextModuleId++;
}

void Engine::setSampleRate(float newSampleRate) {
	internal->sampleRateRequested = newSampleRate;
}

float Engine::getSampleRate() {
	return internal->sampleRate;
}

float Engine::getSampleTime() {
	return internal->sampleTime;
}


} // namespace rack
