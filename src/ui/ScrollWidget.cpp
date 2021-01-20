#include <ui/ScrollWidget.hpp>
#include <context.hpp>


namespace rack {
namespace ui {


// Internal not currently used


ScrollWidget::ScrollWidget() {
	container = new widget::Widget;
	addChild(container);

	horizontalScrollbar = new Scrollbar;
	horizontalScrollbar->vertical = false;
	horizontalScrollbar->hide();
	addChild(horizontalScrollbar);

	verticalScrollbar = new Scrollbar;
	verticalScrollbar->vertical = true;
	verticalScrollbar->hide();
	addChild(verticalScrollbar);
}


void ScrollWidget::scrollTo(math::Rect r) {
	math::Rect bound = math::Rect::fromMinMax(r.getBottomRight().minus(box.size), r.pos);
	offset = offset.clampSafe(bound);
}


math::Rect ScrollWidget::getContainerOffsetBound() {
	math::Rect r;
	r.pos = containerBox.pos;
	r.size = containerBox.size.minus(box.size);
	return r;
}


math::Vec ScrollWidget::getHandleOffset() {
	return offset.minus(containerBox.pos).div(getContainerOffsetBound().size);
}


math::Vec ScrollWidget::getHandleSize() {
	return box.size.div(containerBox.size);
}


void ScrollWidget::draw(const DrawArgs& args) {
	nvgScissor(args.vg, RECT_ARGS(args.clipBox));
	Widget::draw(args);
	nvgResetScissor(args.vg);
}


void ScrollWidget::step() {
	Widget::step();

	// Set containerBox cache
	containerBox = container->getVisibleChildrenBoundingBox();

	// Clamp scroll offset
	math::Rect offsetBounds = getContainerOffsetBound();
	offset = offset.clamp(offsetBounds);

	// Update the container's position from the offset
	container->box.pos = offset.neg().round();

	// Make scrollbars visible only if there is a positive range to scroll.
	if (hideScrollbars) {
		horizontalScrollbar->setVisible(false);
		verticalScrollbar->setVisible(false);
	}
	else {
		horizontalScrollbar->setVisible(offsetBounds.size.x > 0.f);
		verticalScrollbar->setVisible(offsetBounds.size.y > 0.f);
	}

	// Reposition and resize scroll bars
	math::Vec inner = box.size.minus(math::Vec(verticalScrollbar->box.size.x, horizontalScrollbar->box.size.y));
	horizontalScrollbar->box.pos.y = inner.y;
	verticalScrollbar->box.pos.x = inner.x;
	horizontalScrollbar->box.size.x = verticalScrollbar->isVisible() ? inner.x : box.size.x;
	verticalScrollbar->box.size.y = horizontalScrollbar->isVisible() ? inner.y : box.size.y;
}


void ScrollWidget::onButton(const event::Button& e) {
	OpaqueWidget::onButton(e);
	if (e.isConsumed())
		return;

	// Check if scrollable
	math::Rect offsetBound = getContainerOffsetBound();
	if (offsetBound.size.x <= 0.f && offsetBound.size.y <= 0.f)
		return;

	if (e.button == GLFW_MOUSE_BUTTON_MIDDLE) {
		e.consume(this);
	}
}


void ScrollWidget::onDragStart(const event::DragStart& e) {
	// Check if scrollable
	math::Rect offsetBound = getContainerOffsetBound();
	if (offsetBound.size.x <= 0.f && offsetBound.size.y <= 0.f)
		return;

	if (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_MIDDLE) {
		e.consume(this);
	}
}


void ScrollWidget::onDragMove(const event::DragMove& e) {
	if (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_MIDDLE) {
		math::Vec offsetDelta = e.mouseDelta.div(getAbsoluteZoom());
		offset = offset.minus(offsetDelta);
	}
}


void ScrollWidget::onHoverScroll(const event::HoverScroll& e) {
	OpaqueWidget::onHoverScroll(e);
	if (e.isConsumed())
		return;

	int mods = APP->window->getMods();
	// Don't scroll when Ctrl is held because this interferes with RackScrollWidget zooming.
	if ((mods & RACK_MOD_MASK) & RACK_MOD_CTRL)
		return;

	// Check if scrollable
	math::Rect offsetBound = getContainerOffsetBound();
	if (offsetBound.size.x <= 0.f && offsetBound.size.y <= 0.f)
		return;

	math::Vec scrollDelta = e.scrollDelta;
	// Flip coordinates if shift is held
	// Mac (or GLFW?) already does this for us.
#if !defined ARCH_MAC
	if ((mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT)
		scrollDelta = scrollDelta.flip();
#endif

	offset = offset.minus(scrollDelta);
	e.consume(this);
}


void ScrollWidget::onHoverKey(const event::HoverKey& e) {
	OpaqueWidget::onHoverKey(e);
	if (e.isConsumed())
		return;

	// Check if scrollable
	math::Rect offsetBound = getContainerOffsetBound();
	if (offsetBound.size.x <= 0.f && offsetBound.size.y <= 0.f)
		return;

	if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
		if (e.key == GLFW_KEY_PAGE_UP && (e.mods & RACK_MOD_MASK) == 0) {
			offset.y -= box.size.y * 0.5;
			e.consume(this);
		}
		if (e.key == GLFW_KEY_PAGE_UP && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
			offset.x -= box.size.x * 0.5;
			e.consume(this);
		}
		if (e.key == GLFW_KEY_PAGE_DOWN && (e.mods & RACK_MOD_MASK) == 0) {
			offset.y += box.size.y * 0.5;
			e.consume(this);
		}
		if (e.key == GLFW_KEY_PAGE_DOWN && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
			offset.x += box.size.x * 0.5;
			e.consume(this);
		}
		if (e.key == GLFW_KEY_HOME && (e.mods & RACK_MOD_MASK) == 0) {
			math::Rect containerBox = container->getVisibleChildrenBoundingBox();
			offset.y = containerBox.getTop();
			e.consume(this);
		}
		if (e.key == GLFW_KEY_HOME && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
			math::Rect containerBox = container->getVisibleChildrenBoundingBox();
			offset.x = containerBox.getLeft();
			e.consume(this);
		}
		if (e.key == GLFW_KEY_END && (e.mods & RACK_MOD_MASK) == 0) {
			math::Rect containerBox = container->getVisibleChildrenBoundingBox();
			offset.y = containerBox.getBottom();
			e.consume(this);
		}
		if (e.key == GLFW_KEY_END && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
			math::Rect containerBox = container->getVisibleChildrenBoundingBox();
			offset.x = containerBox.getRight();
			e.consume(this);
		}
	}
}


} // namespace ui
} // namespace rack
