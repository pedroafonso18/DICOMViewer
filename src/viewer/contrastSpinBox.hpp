#pragma once

//--------------------------------------------

#include <QDoubleSpinBox>

//--------------------------------------------

namespace DICOMViewer {

	class ContrastSpinBox final : public QDoubleSpinBox {
	public:
		using QDoubleSpinBox::QDoubleSpinBox;

	protected:
		[[nodiscard]] QString textFromValue(double value) const override;
	};

} // namespace DICOMViewer
