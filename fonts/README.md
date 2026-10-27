# Шрифты

JetBrains Mono для текста и интерфейса вшит модулем Glaze (`glaze/fonts/`).

## Jot Icons

`JotIcons.ttf` и `JotIconsPropo.ttf` — только те символы, что рисуют Jot и контролы Glaze
(кнопки, диалоги), вырезанные из JetBrainsMono Nerd Font (обычный вариант и Propo).
Семейства переименованы, чтобы не путаться с установленными шрифтами.

- Значки Nerd Fonts (Font Awesome и др.) — MIT / OFL, см. https://github.com/ryanoasis/nerd-fonts;
  сам шрифт — SIL Open Font License 1.1 (`OFL.txt`).

Добавили новый `\u{f…}` в `qml/` или в `glaze/qml/` — пересоберите шрифты из корня Jot:

```sh
python glaze/tools/subset_icons.py "Jot Icons" fonts \
    JetBrainsMonoNerdFont-Regular.ttf JetBrainsMonoNerdFontPropo-Regular.ttf qml glaze/qml
```
