#include "viewer/contrastInput.hpp"

#include <QLocale>

#include <cmath>

//--------------------------------------------

namespace DICOMViewer {

	ContrastInput::ContrastInput(QWidget* parent)
		: QLineEdit(parent)
	{
		setInputMethodHints(Qt::ImhFormattedNumbersOnly);
	}

	//--------------------------------------------

	std::optional<double> ContrastInput::number() const
	{
		bool parsed{};
		const double value = locale().toDouble(text(), &parsed);
		if (!parsed || !std::isfinite(value)) {
			return std::nullopt;
		}
		return value;
	}

	//--------------------------------------------

	void ContrastInput::showNumber(std::optional<double> value)
	{
		setText(value ? locale().toString(*value, 'g', QLocale::FloatingPointShortest) : QStringLiteral("—"));
	}

} // namespace DICOMViewer
