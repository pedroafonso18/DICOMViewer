#pragma once

//--------------------------------------------

#include "dicomviewer/metadata.hpp"

#include <QDockWidget>

#include <span>

class QFormLayout;

//--------------------------------------------

namespace DICOMViewer {

	class MetadataPanel final : public QDockWidget {
	public:
		explicit MetadataPanel(QWidget* parent = nullptr);

		void setMetadata(const Metadata& metadata, std::span<const ContrastWindow> windows);

	private:
		void addField(const QString& name, const QString& value);

		QFormLayout* fields{};
	};

} // namespace DICOMViewer
