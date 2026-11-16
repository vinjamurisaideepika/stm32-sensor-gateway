#!/usr/bin/env bash
# ship.sh - commit and push this project to GitHub, with Claude Code writing
# the commit message.
#
#   ./ship.sh                      # normal use
#   ./ship.sh "added SPI driver"   # optional hint for the commit message
#
# First run: creates the GitHub repository (public) and pushes.
# Later runs: run the checks, commit with a Claude-written message, push.
# Needs: git, gh (logged in with `gh auth login`), claude (Claude Code).
set -euo pipefail

CHECK_CMD=""          # build/tests that must pass before pushing
DESCRIPTION="STM32F446RE sensor gateway: ADC, I2C, SPI, UART, timers, interrupts, modular HAL-based firmware (ESP32-S3 emulates the sensors)"
HINT="${1:-}"

cd "$(dirname "$0")"
REPO_NAME="$(basename "$PWD")"

# 1) Make sure this folder is a git repository on branch main
if [ ! -d .git ]; then
  git init -b main
fi

# 1b) Put your GitHub username into README badge links (first run only)
if grep -q "GITHUB_USER" README.md 2>/dev/null; then
  GH_USER="$(gh api user -q .login)"
  perl -pi -e "s/GITHUB_USER/$GH_USER/g" README.md
fi

# 2) Run the project's checks; never push broken code
if [ -n "$CHECK_CMD" ]; then
  echo ">> running checks: $CHECK_CMD"
  bash -c "$CHECK_CMD"
fi

# 3) Stage everything (.gitignore keeps build output and models out)
git add -A
if git diff --cached --quiet; then
  echo ">> nothing new to commit"
else
  echo ">> asking Claude Code to write the commit message..."
  claude -p "Look at the staged changes (git diff --cached --stat, then git diff --cached) and create exactly ONE commit. Use a Conventional Commits message: 'type(scope): short summary' (feat, fix, docs, refactor, test, build, chore), then a blank line and 2-5 bullet points saying what changed and why, written for a recruiter reading the history. Follow the rules in CLAUDE.md. Do not stage, unstage or edit files, and do not push. Extra context from the author: ${HINT:-none}" \
    --allowedTools "Bash(git diff *)" "Bash(git status *)" "Bash(git log *)" "Bash(git commit *)" "Read"
  git log -1 --stat | head -20
fi

# 4) Create the GitHub repository on the first run, then push
if ! git remote get-url origin >/dev/null 2>&1; then
  echo ">> creating GitHub repository $REPO_NAME"
  gh repo create "$REPO_NAME" --public --source=. --remote=origin --description "$DESCRIPTION"
fi
git push -u origin main
echo ">> done: $(gh repo view --json url -q .url)"
