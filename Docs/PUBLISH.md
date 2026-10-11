# Publishing to GitHub from Ubuntu (WSL)

Run in any Ubuntu terminal on the PC that has the project. Paste each block as-is.

## 1. One-time tools and sign-in

```bash
sudo apt update && sudo apt install -y git git-lfs gh
git lfs install
gh auth login --hostname github.com --git-protocol https --web
gh auth setup-git
```

## 2. Point at the project

```bash
DV="/mnt/c/Users/aleks/Desktop/Delta Force Dominion Game/DeltaVDominion"
git config --global --add safe.directory "$DV"
git -C "$DV" config core.fileMode false
git config --global user.name >/dev/null || git config --global user.name "Normansrule"
git config --global user.email >/dev/null || git config --global user.email "60046480+Normansrule@users.noreply.github.com"
```

## 3. Create the repo and push (code + Git LFS assets)

```bash
gh repo create Normansrule/DeltaVDominion --public --source "$DV" --remote origin --push \
  --description "DELTA-V: DOMINION - Unreal Engine 5 squad shooter"
```

Use `--private` instead of `--public` to keep it private (the one-line installer then only works for you).

## 4. Publish the downloadable game

```bash
bash "$DV/Scripts/dv.sh" package     # only if Build/Release/DeltaVDominion-Win64.zip is missing or stale
gh release create v0.1.0 "$DV/Build/Release/DeltaVDominion-Win64.zip" --repo Normansrule/DeltaVDominion \
  --title "Boot Camp v0.1.0" --notes "Grey-box Boot Camp slice. Unzip and run DeltaVDominion.exe, or use the one-line installer in the README."
```

## 5. Later updates

```bash
git -C "$DV" add -A && git -C "$DV" commit -m "Describe the change" && git -C "$DV" push
```

New game build: bump the tag, e.g. `gh release create v0.2.0 ...` with the same zip path.

## Watching progress in Unreal

```bash
echo "alias dv='bash \"$DV/Scripts/dv.sh\"'" >> ~/.bashrc && source ~/.bashrc
dv watch     # spectator camera, squad flanks the Hearthbound
dv play      # play it yourself
dv editor    # open the Unreal Editor
dv status    # commits and what's built
dv log       # live log
```
