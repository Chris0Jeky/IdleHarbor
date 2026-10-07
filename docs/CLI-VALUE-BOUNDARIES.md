# Command-line value boundaries

A single-dash filename such as `-odd.ini`, `-j-settings.ini` or `-z` is accepted after
`--config`. Recognized short options (`-h`, `-?`, `-j`, `-g`, `-m`, `-r`, `-o`, `-s`,
`-d`) and tokens starting with `--` remain options, not implicit configuration paths.
Use an explicit relative path, for example `.\--start` or `.\-j`, when a filename
would otherwise be indistinguishable from a reserved option.

A missing value does not consume the following option: `--interval --stop` reports
an error and retains Stop in the parse result. The application still rejects the
errored invocation; retaining the parsed command does not execute it. Numeric and
choice values retain their own validation. An unknown short token outside a value
position still produces an unknown-option error.

`command_line_values` is registered with CTest. Its 109 parse cases cover filenames,
recognized short flags, long-option syntax, missing values and invalid values.
