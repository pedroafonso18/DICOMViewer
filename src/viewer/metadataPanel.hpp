#pragma once

//--------------------------------------------

#include "dicomviewer/metadata.hpp"

#include <QDockWidget>

class QFormLayout;

//--------------------------------------------

namespace DICOMViewer {

	class MetadataPanel final : public QDockWidget {
	public:
		explicit MetadataPanel(QWidget* parent = nullptr);

		void setMetadata(const Metadata& metadata);

	private:
		void addField(const QString& name, const QString& value);

		QFormLayout* fields{};
	};

} // namespace DICOMViewer
