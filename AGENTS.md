# Anaise project rules

## GitHub
- Remote: https://github.com/InvalidoUser/Anaise.git
- This repository is PUBLIC. Treat every committed file and commit message as public.
- Git and GitHub CLI are configured. When the user asks to push, review changes, commit the intended files, then run git push. The default branch is main.

## Credentials and private data
- Never commit API keys, access tokens, passwords, private keys, real environment files, service-account credentials, or personal data.
- Read secrets from local environment variables or ignored .env files. Never hardcode secrets into source code, tests, URLs, documentation, or examples.
- Environment templates (.env.example, .env.sample, .env.template) must contain variable names and obvious dummy placeholders only.
- Do not print secret values in terminal output, logs, diffs, commit messages, or chat.
- Do not force-add ignored files or weaken .gitignore to include credentials.
- .gitignore does not protect files that Git already tracks and does not detect secrets embedded in source code.

## Before each commit and push
- Review git status and git diff --cached, including every newly added file. Prefer staging explicit paths.
- Check staged content for credentials, private data, accidentally included environment files, and generated files. Use a secret scanner when available; manual review is still required.
- If a secret is found, stop the commit or push, remove it from staged changes, and tell the user without exposing its value.
- If a secret was already committed or published, stop pushing and tell the user to revoke or rotate it. Deleting the file in a later commit does not remove the secret from history.
- Do not force-push, rewrite published history, or delete remote branches without explicit user instruction.

## Testing
- Run `powershell -ExecutionPolicy Bypass -File tools\test.ps1` for the checks.
- Capture screens with `tools\headless.ps1` or `bin\Anaise.exe --headless`; both render offscreen and create no window.
- Do not launch the windowed app during automated work. It appears on the desktop and takes focus.

## Orientation
- Start from `.opencode/skills/anaise/SKILL.md` and its `references/PROJECT_MAP.md`. They map `src/main.cpp`, list the invariants and commands, and carry a generated line index, so the source does not have to be read whole.
- Refresh that index after structural changes with `powershell -ExecutionPolicy Bypass -File .opencode\skills\anaise\scripts\map.ps1`.
