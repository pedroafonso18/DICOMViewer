#pragma once

//--------------------------------------------

#include <QLineEdit>

#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	class ContrastInput final : public QLineEdit {
	public:
		explicit ContrastInput(QWidget* parent = nullptr);

		[[nodiscard]] std::optional<double> number() const;
		void showNumber(std::optional<double> value);
	};

} // namespace DICOMViewer
