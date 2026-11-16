#include "viewer/contrastSpinBox.hpp"

#include <QLocale>

//--------------------------------------------

namespace DICOMViewer {

	QString ContrastSpinBox::textFromValue(double value) const
	{
		auto text = QDoubleSpinBox::textFromValue(value);
		const QLocale inputLocale = locale();
		const auto decimal = inputLocale.decimalPoint();
		const auto zero = inputLocale.zeroDigit();
		if (text.contains(decimal)) {
			while (text.endsWith(zero)) {
				text.chop(zero.size());
			}
			if (text.endsWith(decimal)) {
				text.chop(decimal.size());
			}
		}
		return text;
	}

} // namespace DICOMViewer
