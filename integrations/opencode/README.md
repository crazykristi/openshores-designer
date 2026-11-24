# Shipgen Designer for OpenCode

Lets [OpenCode](https://opencode.ai) design ships and buildings in the
Shipgen Designer window through its MCP server, with any model OpenCode
runs, local ones included. Made for OpenCode 2.x (servers under
`mcp.servers`).

## Set up

1. Copy this folder somewhere to work in, for example `~/shipgen-opencode`.
2. In `opencode.json`, set the path in `command` to your designer:
   the AppImage on Linux, `C:\\path\\to\\shipgen-designer.exe` on Windows
   (backslashes doubled), or
   `/Applications/Shipgen Designer.app/Contents/MacOS/shipgen-designer` on macOS.
3. Start `opencode` in that folder. The designer window opens; OpenCode
   starts it and stops it.

Or add the server to a project of your own with one command, which writes
the right format for your OpenCode version:

```
opencode mcp add shipgen -- /path/to/shipgen-designer-x86_64.AppImage --mcp --mcp-folder designs
```

`opencode mcp list` should then say `shipgen  connected`.

## What is in it

* `opencode.json`: the MCP server. `codemode: false` gives the model the
  designer's tools directly, which suits smaller local models best; the
  startup timeout gives the window a minute to open.
* `.opencode/agents/shipgen.md`: the **shipgen** agent (the default here),
  with the plan's axes and units, the usual order of work, and the rules a
  design must meet. It may not edit files or run commands: it works only
  through the designer.
* `.opencode/commands/ship.md`: `/ship <description>`, for example
  `/ship a small courier with two decks and a cargo hold`.
* `designs/`: the only folder the designer lets the AI read and write.
  Put 3D models here for `import_model`; plans and blueprints are saved here.

## Tips for local models

* Models with good tool calling work best (Qwen 2.5/3 Coder, Llama 3.1+,
  Mistral Small). Give them a context of 16k tokens or more: the tool list
  and plan summaries are long.
* Ask for something small first and let it build; then ask for more rooms.
* If it keeps getting a tool wrong, the designer's answer says why: tell it
  to read the error.

## Troubleshooting

* `opencode mcp list` says *No MCP servers configured* the first time it
  runs in a new folder: run it again (OpenCode reads a new project's config
  on the second start).
* The window does not open: check the path in `command`, and on Linux that
  the AppImage is executable (`chmod +x`).
* `opencode mcp list` shows the server as failed: start the designer with
  `--mcp` yourself in a terminal; it prints what went wrong.
