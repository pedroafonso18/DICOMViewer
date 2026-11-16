#include "viewer/mainWindow.hpp"

#include <QApplication>

//--------------------------------------------

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
	application.setApplicationName(QStringLiteral("DICOM Viewer"));

	DICOMViewer::MainWindow window;
	window.show();
	return application.exec();
}
