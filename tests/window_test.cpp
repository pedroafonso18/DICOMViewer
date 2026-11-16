#include "dicomviewer/window.hpp"

#include <gtest/gtest.h>

#include <limits>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		TEST(ContrastWindowTest, AcceptsFiniteLevelsAndWidthsOfAtLeastOne)
		{
			EXPECT_TRUE((ContrastWindow{-100.0, 1.0}.isValid()));
			EXPECT_TRUE((ContrastWindow{0.5, 20.0}.isValid()));
		}

		//--------------------------------------------

		TEST(ContrastWindowTest, RejectsNonFiniteValuesAndSubunitWidths)
		{
			constexpr double infinity = std::numeric_limits<double>::infinity();
			constexpr double nan = std::numeric_limits<double>::quiet_NaN();
			EXPECT_FALSE((ContrastWindow{0.0, 0.5}.isValid()));
			EXPECT_FALSE((ContrastWindow{0.0, -1.0}.isValid()));
			EXPECT_FALSE((ContrastWindow{infinity, 2.0}.isValid()));
			EXPECT_FALSE((ContrastWindow{0.0, nan}.isValid()));
			EXPECT_FALSE((ContrastWindow{0.0, infinity}.isValid()));
		}

	} // namespace
} // namespace DICOMViewer
