//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - the version of the release, shared by the designer, shipgen-gui
// and the command line. package-appimage.sh reads it for the file name.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_VERSION_H
#define SHIPGEN_VERSION_H

#define SHIPGEN_VERSION "0.3.7-beta-rc2"
// Where problems are reported (shown when the program finds something missing).
#define SHIPGEN_ISSUES_URL "https://github.com/crazykristi/openshores-designer/issues"
#define SHIPGEN_AUTHOR "Kristiana Moretti"

// The text of an About box: the program, version, author and when this copy
// was built. Each program compiles it in its own files, which build.sh
// compiles afresh every build, so __DATE__ and __TIME__ are that build's.
#define SHIPGEN_ABOUT_HTML(program, what) \
	("<h3>" program " " SHIPGEN_VERSION "</h3>" \
	 "<p>" what "</p>" \
	 "<p>By " SHIPGEN_AUTHOR ".</p>" \
	 "<p>Built " __DATE__ " at " __TIME__ ".</p>")

#endif
