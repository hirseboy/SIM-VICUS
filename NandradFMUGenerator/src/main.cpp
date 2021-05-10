#include <QApplication>

#include <QFileInfo>
#include <QDir>

#include <IBK_Exception.h>
#include <IBK_ArgParser.h>

#include "NandradFMUGeneratorWidget.h"

int main(int argc, char *argv[]) {
	FUNCID(main);

	// create QApplication
	QApplication a(argc, argv);

	// *** Locale setup for Unix/Linux ***
#if defined(Q_OS_UNIX)
	setlocale(LC_NUMERIC,"C");
#endif

	qApp->setApplicationName("NANDRAD FMU Generator");

	// disable ? button in windows
#if QT_VERSION >= 0x050A00
	QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#elif QT_VERSION >= 0x050600
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

	IBK::ArgParser args;
	args.m_appname = "NandradFMUGenerator";
	args.addOption(0, "generate", "Generates this FMU from the provided project file (requires project file argument).", "fmu-filename", "");
	args.parse(argc, argv);

	// *** Setup and show main widget and start event loop ***
	int res;
	try {

		NandradFMUGeneratorWidget w;

		// TODO : For now we assume install dir to be same as NandradSolver dir
		w.m_installDir					= QFileInfo(argv[0]).dir().absolutePath();
		w.m_nandradSolverExecutable		= QFileInfo(argv[0]).dir().absoluteFilePath("NandradSolver");

		// if started as: NandradFMUGenerator /path/to/projectFile.nandrad
		// we copy /path/to/projectFile.nandrad as path to NANDRAD
		if (args.args().size() > 0) {
			std::string projectFile = args.args()[0];
			if (projectFile.find("file://") == 0)
				projectFile = projectFile.substr(7);
			w.m_nandradFilePath = IBK::Path(projectFile);
		}
		if (args.hasOption("generate")) {
			if (!w.m_nandradFilePath.isValid()) {
				throw IBK::Exception("Oroject file argument expected when using '--generate'.", FUNC_ID);
			}
			w.m_silent = true; // set widget into silent mode
			w.init(); // populate line edits
			if (w.setup() != 0) // try to read project
				return EXIT_FAILURE;
			// override default model name
			w.setModelName(QString::fromStdString(args.option("generate")));
			// generate FMU and return
			return w.generate();
		}

		w.init();
		w.resize(1600,800);
		w.show(); // show widget

		// start event loop
		res = a.exec();

	} // here our widget dies, main window goes out of scope and UI goes down -> destructor does ui and thread cleanup
	catch (IBK::Exception & ex) {
		ex.writeMsgStackToError();
		return EXIT_FAILURE;
	}

	// return exit code to environment
	return res;
}
