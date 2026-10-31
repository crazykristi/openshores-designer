//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// shipgen - message handling shared by the command line and the tools.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef SHIPGEN_LOGGING_H
#define SHIPGEN_LOGGING_H

// The engine warns when its obstruction cache is refreshed outside the game's
// scene client. An offline tool is never a scene client, yet it has to build
// that cache: the hull volume and the engine's own report are read from it.
// The cache is built correctly, so this one known warning is dropped instead
// of being printed every time a design is inspected or validated. Every other
// message, including real engine errors, is passed through untouched.
void	InstallShipgenMessageHandler();

#endif // SHIPGEN_LOGGING_H
