# Privacy

Short version: the only network call this tool makes is fetching game
offsets at startup. No telemetry, no analytics, no accounts.

## The offset fetch

On startup, the tool requests `offsets.json` from the configured
offset service (default: `https://offsets.imtheo.lol`, run by imtheo —
not by me). That request is a plain fetch of a JSON file. The service
operator sees what any web server sees: your IP, the request time,
the file asked for. Nothing about your system, your configs, or what
you do with the tool is sent.

If you'd rather not talk to a third party at all: put an `offsets.json`
in the working directory and the tool uses it as fallback, or point
`offsets_base_url` at your own service. Both are documented in the
README.

## Everything else stays local

- Configs are JSON files on your disk. They never leave your machine.
- No crash reporting, no usage stats, no update checks phoning home.
- The tool reads the Roblox process's memory and draws an overlay.
  That data is rendered on your screen and discarded — it's not
  logged, uploaded, or stored anywhere.

## If you build from source

Same deal. The network behavior is in `src/` for anyone to read —
search for the offset loader if you want to verify exactly what's
fetched and when.
