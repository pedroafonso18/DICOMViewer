#include "viewer/metadataPanel.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStringList>

//--------------------------------------------

namespace DICOMViewer {

	MetadataPanel::MetadataPanel(QWidget* parent)
		: QDockWidget(tr("Metadata"), parent)
	{
		setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
		auto* scroll = new QScrollArea(this);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		auto* content = new QWidget(scroll);
		fields = new QFormLayout(content);
		fields->setRowWrapPolicy(QFormLayout::WrapLongRows);
		scroll->setWidget(content);
		setWidget(scroll);
		addField(tr("Image"), tr("No file open"));
	}

	//--------------------------------------------

	void MetadataPanel::setMetadata(const Metadata& metadata, std::span<const ContrastWindow> windows)
	{
		while (fields->rowCount() > 0) {
			fields->removeRow(0);
		}

		addField(tr("Modality"), QString::fromStdString(metadata.modality));
		addField(tr("Dimensions"), tr("%1 × %2 px").arg(metadata.width).arg(metadata.height));
		addField(tr("Frames"), QString::number(metadata.frameCount));
		auto spacing = QString::fromStdString(metadata.pixelSpacing);
		if (!spacing.isEmpty()) {
			spacing.replace(QLatin1Char('\\'), QStringLiteral(" × "));
			addField(tr("Pixel spacing"), tr("%1 mm").arg(spacing));
		}
		addField(tr("Photometric"), QString::fromStdString(metadata.photometricInterpretation));
		addField(tr("Bits stored"), QString::number(metadata.bitsStored));
		addField(tr("Transfer syntax"), QString::fromStdString(metadata.transferSyntaxName));
		addField(tr("Transfer syntax UID"), QString::fromStdString(metadata.transferSyntax));
		addField(tr("Patient orientation"), QString::fromStdString(metadata.orientation));
		addField(tr("Image orientation"), QString::fromStdString(metadata.imageOrientation));
		addField(tr("Laterality"), QString::fromStdString(metadata.laterality));
		addField(tr("View position"), QString::fromStdString(metadata.viewPosition));

		QStringList presets;
		for (const auto& window : windows) {
			presets.append(tr("%1 / %2").arg(window.windowLevel).arg(window.width));
		}
		addField(tr("Stored level / width"), presets.join('\n'));
	}

	//--------------------------------------------

	void MetadataPanel::addField(const QString& name, const QString& value)
	{
		if (value.isEmpty()) {
			return;
		}
		auto* label = new QLabel;
		label->setTextFormat(Qt::PlainText);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		label->setWordWrap(true);
		label->setText(value);
		fields->addRow(name, label);
	}

} // namespace DICOMViewer
