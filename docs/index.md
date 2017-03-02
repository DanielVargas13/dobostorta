---
title: Dobostorta browser
---

The minimal browser, **Dobostorta**.

# How to install
Now, Dobostorta don't distribute binary files. Please compile in your computer.
```
$ git clone http://github.com/macrat/dobostorta.git
$ cd dobostorta
$ qmake
$ make
$ sudo make install
```

## Uninstall
You can uninstall binary with `sudo make uninstall`.
And, remove data files (maybe saved on `~/.local/share/dobostorta`).

## Usae
### Shortcuts
#### Scroll
<table>
<tr><th>Key</th><th>Description</th></tr>
<tr><td>Ctrl-k or up</td><td>Scroll to up. </td></tr>
<tr><td>Ctrl-j or down</td><td>Scroll to down.</td></tr>
<tr><td>Ctrl-h or left</td><td>Scroll to left.</td></tr>
<tr><td>Ctrl-l or right</td><td>Scroll to right.</td></tr>
<tr><td>Ctrl-g Ctrl-g or Home</td><td>Scroll to page top.</td></tr>
<tr><td>Ctrl-G or End</td><td>Scroll to page bottom.</td></tr>
</table>

#### Zoom
<table>
<tr><th>Key</th><th>Description</th></tr>
<tr><td>Ctrl-Plus</td><td>Zoom in.</td></tr>
<tr><td>Ctrl-Minus</td><td>Zoom out.</td></tr>
<tr><td>Ctrl-0</td><td>Reset zoom level.</td></tr>
</table>

#### History
<table>
<tr><th>Key</th><th>Description</th></tr>
<tr><td>Ctrl-i or Alt-Right</td><td>Forward history.</td></tr>
<tr><td>Ctrl-o or Alt-Left</td><td>Back history.</td></tr>
<tr><td>Ctrl-r</td><td>Reload page.</td></tr>
</table>

#### Bar
<table>
<tr><th>Key</th><th>Description</th></tr>
<tr><td>Ctrl-:</td><td>Focus to bar and start edit url or web search.</td></tr>
<tr><td>Ctrl-/</td><td>Focus to bar and start in-site search.</td></tr>
<tr><td>Ctrl-[ or escape</td><td>Escape from bar and close. Only working when bar has focus.</td></tr>
<tr><td>Ctrl-F</td><td>Find next text. Only working when inputted in-site search query into bar.</td></tr>
<tr><td>Ctrl-P</td><td>Find previous text. Only working when inputted in-site search query into bar.</td></tr>
</table>

#### Others
<table>
<tr><th>Key</th><th>Description</th></tr>
<tr><td>Ctrl-N</td><td>Open new window.</td></tr>
<tr><td>Ctrl-P</td><td>Open new incognito window.</td></tr>
</table>
