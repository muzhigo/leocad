#include "lc_global.h"
#include <stdio.h>
#include "lc_application.h"
#include "lc_library.h"
#include "lc_profile.h"
#include "project.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"
#include "lc_shortcuts.h"
#include "view.h"

lcApplication* gApplication;

void lcPreferences::LoadDefaults()
{
	mFixedAxes = lcGetProfileInt(LC_PROFILE_FIXED_AXES);
	mMouseSensitivity = lcGetProfileInt(LC_PROFILE_MOUSE_SENSITIVITY);
	mShadingMode = (lcShadingMode)lcGetProfileInt(LC_PROFILE_SHADING_MODE);
	mDrawAxes = lcGetProfileInt(LC_PROFILE_DRAW_AXES);
	mDrawEdgeLines = lcGetProfileInt(LC_PROFILE_DRAW_EDGE_LINES);
	mLineWidth = lcGetProfileFloat(LC_PROFILE_LINE_WIDTH);
	mDrawGridStuds = lcGetProfileInt(LC_PROFILE_GRID_STUDS);
	mGridStudColor = lcGetProfileInt(LC_PROFILE_GRID_STUD_COLOR);
	mDrawGridLines = lcGetProfileInt(LC_PROFILE_GRID_LINES);
	mGridLineSpacing = lcGetProfileInt(LC_PROFILE_GRID_LINE_SPACING);
	mGridLineColor = lcGetProfileInt(LC_PROFILE_GRID_LINE_COLOR);
}

void lcPreferences::SaveDefaults()
{
	lcSetProfileInt(LC_PROFILE_FIXED_AXES, mFixedAxes);
	lcSetProfileInt(LC_PROFILE_MOUSE_SENSITIVITY, mMouseSensitivity);
	lcSetProfileInt(LC_PROFILE_SHADING_MODE, mShadingMode);
	lcSetProfileInt(LC_PROFILE_DRAW_AXES, mDrawAxes);
	lcSetProfileInt(LC_PROFILE_DRAW_EDGE_LINES, mDrawEdgeLines);
	lcSetProfileFloat(LC_PROFILE_LINE_WIDTH, mLineWidth);
	lcSetProfileInt(LC_PROFILE_GRID_STUDS, mDrawGridStuds);
	lcSetProfileInt(LC_PROFILE_GRID_STUD_COLOR, mGridStudColor);
	lcSetProfileInt(LC_PROFILE_GRID_LINES, mDrawGridLines);
	lcSetProfileInt(LC_PROFILE_GRID_LINE_SPACING, mGridLineSpacing);
	lcSetProfileInt(LC_PROFILE_GRID_LINE_COLOR, mGridLineColor);
}

lcApplication::lcApplication(int Argc, char** Argv)
	: QApplication(Argc, Argv)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
	setApplicationDisplayName("LeoCAD");
#endif

	setOrganizationDomain("leocad.org");
	setOrganizationName("LeoCAD Software");
	setApplicationName("LeoCAD");
	setApplicationVersion(LC_VERSION_TEXT);

	gApplication = this;
	mProject = nullptr;
	mLibrary = nullptr;

	mPreferences.LoadDefaults();
}

lcApplication::~lcApplication()
{
    delete mProject;
    delete mLibrary;
	gApplication = nullptr;
}

void lcApplication::SetProject(Project* Project)
{
	delete mProject;
	mProject = Project;

	gMainWindow->RemoveAllModelTabs();

	Project->SetActiveModel(0);
	lcGetPiecesLibrary()->RemoveTemporaryPieces();
}

void lcApplication::SetClipboard(const QByteArray& Clipboard)
{
	mClipboard = Clipboard;
	gMainWindow->UpdatePaste(!mClipboard.isEmpty());
}

void lcApplication::ExportClipboard(const QByteArray& Clipboard)
{
	QMimeData* MimeData = new QMimeData();

	MimeData->setData("application/vnd.leocad-clipboard", Clipboard);
	QApplication::clipboard()->setMimeData(MimeData);

	SetClipboard(Clipboard);
}

bool lcApplication::LoadPartsLibrary(const QList<QPair<QString, bool>>& LibraryPaths, bool OnlyUsePaths, bool ShowProgress)
{
	if (mLibrary == nullptr)
		mLibrary = new lcPiecesLibrary();

	if (!OnlyUsePaths)
	{
		char* EnvPath = getenv("LEOCAD_LIB");

		if (EnvPath && EnvPath[0])
			return mLibrary->Load(EnvPath, ShowProgress);

		QString CustomPath = lcGetProfileString(LC_PROFILE_PARTS_LIBRARY);

		if (!CustomPath.isEmpty())
			return mLibrary->Load(CustomPath, ShowProgress);
	}

	for (const QPair<QString, bool>& LibraryPathEntry : LibraryPaths)
	{
		if (mLibrary->Load(LibraryPathEntry.first, ShowProgress))
		{
			if (LibraryPathEntry.second)
				mLibrary->SetOfficialPieces();

			return true;
		}
	}

	return false;
}

bool lcApplication::Initialize(QList<QPair<QString, bool>>& LibraryPaths, bool& ShowWindow)
{
	bool OnlyUseLibraryPaths = false;
	bool SaveImage = false;
	bool SaveWavefront = false;
	bool Save3DS = false;
	bool SaveCOLLADA = false;
	bool Orthographic = false;
	bool ImageHighlight = false;
	int ImageWidth = lcGetProfileInt(LC_PROFILE_IMAGE_WIDTH);
	int ImageHeight = lcGetProfileInt(LC_PROFILE_IMAGE_HEIGHT);
	int ImageStart = 0;
	int ImageEnd = 0;
	QString ImageName;
	QString ModelName;
	QString CameraName;
	QString ViewpointName;
	QString ProjectName;
	QString SaveWavefrontName;
	QString Save3DSName;
	QString SaveCOLLADAName;

	QStringList Arguments = arguments();
	const int NumArguments = Arguments.size();

	for (int ArgIdx = 1; ArgIdx < NumArguments; ArgIdx++)
	{
		const QString& Param = Arguments[ArgIdx];

		if (Param[0] != '-')
		{
			ProjectName = Param;
			continue;
		}

		auto ParseString = [&ArgIdx, &Arguments, NumArguments](QString& Value, bool Required)
		{
			if (ArgIdx < NumArguments - 1 && Arguments[ArgIdx + 1][0] != '-')
			{
				ArgIdx++;
				Value = Arguments[ArgIdx];
			}
			else if (Required)
				printf("Not enough parameters for the '%s' argument.\n", Arguments[ArgIdx].toLatin1().constData());
		};

		auto ParseInteger = [&ArgIdx, &Arguments, NumArguments](int& Value)
		{
			if (ArgIdx < NumArguments - 1 && Arguments[ArgIdx + 1][0] != '-')
			{
				bool Ok = false;
				ArgIdx++;
				int NewValue = Arguments[ArgIdx].toInt(&Ok);

				if (Ok)
					Value = NewValue;
				else
					printf("Invalid value specified for the '%s' argument.\n", Arguments[ArgIdx - 1].toLatin1().constData());
			}
			else
				printf("Not enough parameters for the '%s' argument.\n", Arguments[ArgIdx].toLatin1().constData());
		};

		if (Param == QLatin1String("-l") || Param == QLatin1String("--libpath"))
		{
			QString LibPath;
			ParseString(LibPath, true);
			if (!LibPath.isEmpty())
			{
				LibraryPaths.clear();
				LibraryPaths += qMakePair<QString, bool>(LibPath, false);
				OnlyUseLibraryPaths = true;
			}
		}
		else if (Param == QLatin1String("-i") || Param == QLatin1String("--image"))
		{
			SaveImage = true;
			ParseString(ImageName, false);
		}
		else if (Param == QLatin1String("-w") || Param == QLatin1String("--width"))
			ParseInteger(ImageWidth);
		else if (Param == QLatin1String("-h") || Param == QLatin1String("--height"))
			ParseInteger(ImageHeight);
		else if (Param == QLatin1String("-f") || Param == QLatin1String("--from"))
			ParseInteger(ImageStart);
		else if (Param == QLatin1String("-t") || Param == QLatin1String("--to"))
			ParseInteger(ImageEnd);
		else if (Param == QLatin1String("-m") || Param == QLatin1String("--model"))
			ParseString(ModelName, true);
		else if (Param == QLatin1String("-c") || Param == QLatin1String("--camera"))
			ParseString(CameraName, true);
		else if (Param == QLatin1String("--viewpoint"))
			ParseString(ViewpointName, true);
		else if (Param == QLatin1String("--orthographic"))
			Orthographic = true;
		else if (Param == QLatin1String("--highlight"))
			ImageHighlight = true;
		else if (Param == QLatin1String("-obj") || Param == QLatin1String("--export-wavefront"))
		{
			SaveWavefront = true;
			ParseString(SaveWavefrontName, false);
		}
		else if (Param == QLatin1String("-3ds") || Param == QLatin1String("--export-3ds"))
		{
			Save3DS = true;
			ParseString(Save3DSName, false);
		}
		else if (Param == QLatin1String("-dae") || Param == QLatin1String("--export-collada"))
		{
			SaveCOLLADA = true;
			ParseString(SaveCOLLADAName, false);
		}
		else if (Param == QLatin1String("-v") || Param == QLatin1String("--version"))
		{
			printf("LeoCAD Version " LC_VERSION_TEXT "\n");
			printf("Compiled " __DATE__ "\n");

			ShowWindow = false;
			return true;
		}
		else if (Param == QLatin1String("-?") || Param == QLatin1String("--help"))
		{
			printf("Usage: leocad [options] [file]\n");
			printf("  [options] can be:\n");
			printf("  -l, --libpath <path>: Loads the Pieces Library from path.\n");
			printf("  -i, --image <outfile.ext>: Saves a picture in the format specified by ext.\n");
			printf("  -w, --width <width>: Sets the picture width.\n");
			printf("  -h, --height <height>: Sets the picture height.\n");
			printf("  -f, --from <time>: Sets the first frame or step to save pictures.\n");
			printf("  -t, --to <time>: Sets the last frame or step to save pictures.\n");
			printf("  -m, --model <model>: Sets the active submodel.\n");
			printf("  -c, --camera <camera>: Sets the active camera.\n");
			printf("  --viewpoint (front|back|left|right|top|bottom|home): Sets the viewpoint.\n");
			printf("  --orthographic: Make the view orthographic.\n");
			printf("  --highlight: Highlight pieces in the steps they appear.\n");
			printf("  -obj, --export-wavefront <outfile.obj>: Exports the model to Wavefront OBJ format.\n");
			printf("  -3ds, --export-3ds <outfile.3ds>: Exports the model to 3D Studio 3DS format.\n");
			printf("  -dae, --export-collada <outfile.dae>: Exports the model to COLLADA DAE format.\n");
			printf("  -v, --version: Output version information and exit.\n");
			printf("  -?, --help: Display this help and exit.\n");
			printf("  \n");

			ShowWindow = false;
			return true;
		}
		else
			printf("Unknown parameter: '%s'\n", Param.toLatin1().constData());
	}

	gMainWindow = new lcMainWindow();
	lcLoadDefaultKeyboardShortcuts();
	lcLoadDefaultMouseShortcuts();

	ShowWindow = !SaveImage && !SaveWavefront && !Save3DS && !SaveCOLLADA;

	if (!LoadPartsLibrary(LibraryPaths, OnlyUseLibraryPaths, ShowWindow))
	{
		QString Message;

		if (mLibrary->LoadBuiltinPieces())
			Message = tr("LeoCAD could not find a compatible Parts Library so only a small number of parts will be available.\n\nPlease visit http://www.leocad.org for information on how to download and install a library.");
		else
			Message = tr("LeoCAD could not load Parts Library.\n\nPlease visit http://www.leocad.org for information on how to download and install a library.");

		if (ShowWindow)
			QMessageBox::information(gMainWindow, tr("LeoCAD"), Message);
		else
			fprintf(stderr, "%s", Message.toLatin1().constData());
	}

	gMainWindow->CreateWidgets();

	Project* NewProject = new Project();
	SetProject(NewProject);

	if (!ProjectName.isEmpty() && gMainWindow->OpenProject(ProjectName))
	{
		if (!ModelName.isEmpty())
			lcGetActiveProject()->SetActiveModel(ModelName);

		if (!CameraName.isEmpty())
		{
			gMainWindow->GetActiveView()->SetCamera(CameraName.toLatin1()); // todo: qstring
			if (!ViewpointName.isEmpty() || Orthographic)
				printf("Warning: --viewpoint and --orthographic are ignored when --camera is set.\n");
		}
		else
		{
			if (!ViewpointName.isEmpty())
			{
				if (ViewpointName == QLatin1String("front"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_FRONT);
				else if (ViewpointName == QLatin1String("back"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_BACK);
				else if (ViewpointName == QLatin1String("top"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_TOP);
				else if (ViewpointName == QLatin1String("bottom"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_BOTTOM);
				else if (ViewpointName == QLatin1String("left"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_LEFT);
				else if (ViewpointName == QLatin1String("right"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_RIGHT);
				else if (ViewpointName == QLatin1String("home"))
					gMainWindow->GetActiveView()->SetViewpoint(LC_VIEWPOINT_HOME);
				else
					printf("Unknown viewpoint: '%s'\n", ViewpointName.toLatin1().constData());
			}
			gMainWindow->GetActiveView()->SetProjection(Orthographic);
		}

		if (SaveImage)
		{
			QString FileName;

			if (!ImageName.isEmpty())
				FileName = ImageName;
			else
			{
				FileName = ProjectName;
				QString Extension = QFileInfo(FileName).suffix();
				if (!Extension.isEmpty())
					FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += lcGetProfileString(LC_PROFILE_IMAGE_EXTENSION);
			}

			if (ImageEnd < ImageStart)
				ImageEnd = ImageStart;
			else if (ImageStart > ImageEnd)
				ImageStart = ImageEnd;

			if ((ImageStart == 0) && (ImageEnd == 0))
				ImageStart = ImageEnd = mProject->GetActiveModel()->GetCurrentStep();
			else if ((ImageStart == 0) && (ImageEnd != 0))
				ImageStart = ImageEnd;
			else if ((ImageStart != 0) && (ImageEnd == 0))
				ImageEnd = ImageStart;

			if (ImageStart > 255)
				ImageStart = 255;

			if (ImageEnd > 255)
				ImageEnd = 255;

			QString Frame;

			if (ImageStart != ImageEnd)
			{
				QString Extension = QFileInfo(FileName).suffix();
				Frame = FileName.left(FileName.length() - Extension.length() - 1) + QLatin1String("%1.") + Extension;
			}
			else
				Frame = FileName;

			lcGetActiveModel()->SaveStepImages(Frame, ImageStart != ImageEnd, CameraName == nullptr, ImageHighlight, ImageWidth, ImageHeight, ImageStart, ImageEnd);
		}

		if (SaveWavefront)
		{
			QString FileName;

			if (!SaveWavefrontName.isEmpty())
				FileName = SaveWavefrontName;
			else
				FileName = ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".obj";
			}
			else if (Extension != "obj")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".obj";
			}

			mProject->ExportWavefront(FileName);
		}

		if (Save3DS)
		{
			QString FileName;

			if (!Save3DSName.isEmpty())
				FileName = Save3DSName;
			else
				FileName = ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".3ds";
			}
			else if (Extension != "3ds")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".3ds";
			}

			mProject->Export3DStudio(FileName);
		}

		if (SaveCOLLADA)
		{
			QString FileName;

			if (!SaveCOLLADAName.isEmpty())
				FileName = SaveCOLLADAName;
			else
				FileName = ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".dae";
			}
			else if (Extension != "dae")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".dae";
			}

			mProject->ExportCOLLADA(FileName);
		}
	}

	if (ShowWindow)
	{
		gMainWindow->SetColorIndex(lcGetColorIndex(4));
		gMainWindow->GetPartSelectionWidget()->SetDefaultPart();
		gMainWindow->UpdateRecentFiles();
		gMainWindow->show();
	}

	return true;
}

void lcApplication::Shutdown()
{
	delete mLibrary;
	mLibrary = nullptr;
}

void lcApplication::ShowPreferencesDialog()
{
	lcPreferencesDialogOptions Options;
	int CurrentAASamples = lcGetProfileInt(LC_PROFILE_ANTIALIASING_SAMPLES);

	Options.Preferences = mPreferences;

	Options.DefaultAuthor = lcGetProfileString(LC_PROFILE_DEFAULT_AUTHOR_NAME);
	Options.LibraryPath = lcGetProfileString(LC_PROFILE_PARTS_LIBRARY);
	Options.POVRayPath = lcGetProfileString(LC_PROFILE_POVRAY_PATH);
	Options.LGEOPath = lcGetProfileString(LC_PROFILE_POVRAY_LGEO_PATH);
	Options.CheckForUpdates = lcGetProfileInt(LC_PROFILE_CHECK_UPDATES);

	Options.AASamples = CurrentAASamples;

	Options.Categories = gCategories;
	Options.CategoriesModified = false;
	Options.CategoriesDefault = false;

	Options.KeyboardShortcuts = gKeyboardShortcuts;
	Options.KeyboardShortcutsModified = false;
	Options.KeyboardShortcutsDefault = false;
	Options.MouseShortcuts = gMouseShortcuts;
	Options.MouseShortcutsModified = false;
	Options.MouseShortcutsDefault = false;

	if (!gMainWindow->DoDialog(LC_DIALOG_PREFERENCES, &Options))
		return;

	bool LibraryChanged = Options.LibraryPath != lcGetProfileString(LC_PROFILE_PARTS_LIBRARY);
	bool AAChanged = CurrentAASamples != Options.AASamples;

	mPreferences = Options.Preferences;

	mPreferences.SaveDefaults();

	lcSetProfileString(LC_PROFILE_DEFAULT_AUTHOR_NAME, Options.DefaultAuthor);
	lcSetProfileString(LC_PROFILE_PARTS_LIBRARY, Options.LibraryPath);
	lcSetProfileString(LC_PROFILE_POVRAY_PATH, Options.POVRayPath);
	lcSetProfileString(LC_PROFILE_POVRAY_LGEO_PATH, Options.LGEOPath);
	lcSetProfileInt(LC_PROFILE_CHECK_UPDATES, Options.CheckForUpdates);
	lcSetProfileInt(LC_PROFILE_ANTIALIASING_SAMPLES, Options.AASamples);

	if (LibraryChanged && AAChanged)
		QMessageBox::information(gMainWindow, tr("LeoCAD"), tr("Parts library and Anti-aliasing changes will only take effect the next time you start LeoCAD."));
	else if (LibraryChanged)
		QMessageBox::information(gMainWindow, tr("LeoCAD"), tr("Parts library changes will only take effect the next time you start LeoCAD."));
	else if (AAChanged)
		QMessageBox::information(gMainWindow, tr("LeoCAD"), tr("Anti-aliasing changes will only take effect the next time you start LeoCAD."));

	if (Options.CategoriesModified)
	{
		if (Options.CategoriesDefault)
			lcResetDefaultCategories();
		else
		{
			gCategories = Options.Categories;
			lcSaveDefaultCategories();
		}

		gMainWindow->UpdateCategories();
	}

	if (Options.KeyboardShortcutsModified)
	{
		if (Options.KeyboardShortcutsDefault)
			lcResetDefaultKeyboardShortcuts();
		else
		{
			gKeyboardShortcuts = Options.KeyboardShortcuts;
			lcSaveDefaultKeyboardShortcuts();
		}

		gMainWindow->UpdateShortcuts();
	}

	if (Options.MouseShortcutsModified)
	{
		if (Options.MouseShortcutsDefault)
			lcResetDefaultMouseShortcuts();
		else
		{
			gMouseShortcuts = Options.MouseShortcuts;
			lcSaveDefaultMouseShortcuts();
		}
	}

	// TODO: printing preferences
	/*
	strcpy(opts.strFooter, m_strFooter);
	strcpy(opts.strHeader, m_strHeader);
	*/

	gMainWindow->SetShadingMode(Options.Preferences.mShadingMode);
	gMainWindow->UpdateAllViews();
}
