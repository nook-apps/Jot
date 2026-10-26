# Шрифты

`JetBrainsMono-*.otf` — JetBrains Mono 2.304 (Regular, Italic, SemiBold, Bold, BoldItalic): текст
dокумента и интерфейс. Вшит, чтобы Jot выглядел одинаково на любом дистрибутиве и на Windows.

## Jot Icons

`JotIcons.ttf` и `JotIconsPropo.ttf` — только те символы, что Jot рисует в интерфейсе
(кнопки, диалоги), вырезанные из JetBrainsMono Nerd Font (обычный вариант и Propo) скриптом
`tools/subset_icons.py`. Семейства переименованы, чтобы не путаться с установленными шрифтами.

- JetBrains Mono — SIL Open Font License 1.1 (`OFL.txt`).
- Значки Nerd Fonts (Font Awesome и др.) — MIT / OFL, см. https://github.com/ryanoasis/nerd-fonts.

Добавили новый `\u{f…}` в qml/ — пересоберите шрифты тем же скриптом.
