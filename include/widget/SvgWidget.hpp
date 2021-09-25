#pragma once
#include <widget/Widget.hpp>


namespace rack {
namespace widget {


/** Draws an Svg */
struct SvgWidget : Widget {
	std::shared_ptr<window::Svg> svg;

	/** Sets the box size to the SVG image size */
	void wrap();

	/** Sets and wraps the SVG */
	void setSvg(std::shared_ptr<window::Svg> svg);
	DEPRECATED void setSVG(std::shared_ptr<window::Svg> svg) {
		setSvg(svg);
	}

	void draw(const DrawArgs& args) override;
};


DEPRECATED typedef SvgWidget SVGWidget;


} // namespace widget
} // namespace rack
