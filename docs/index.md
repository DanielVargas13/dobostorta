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
|Key                  |Description           |
|---------------------|----------------------|
|Ctrl-k or up         |Scroll to up.         |
|Ctrl-j or down       |Scroll to down.       |
|Ctrl-h or left       |Scroll to left.       |
|Ctrl-l or right      |Scroll to right.      |
|Ctrl-g Ctrl-g or Home|Scroll to page top.   |
|Ctrl-G or End        |Scroll to page bottom.|

#### Zoom
|Key       |Description     |
|----------|----------------|
|Ctrl-Plus |Zoom in         |
|Ctrl-Minus|Zoom out        |
|Ctrl-0    |Reset zoom level|

#### History
|Key                |Description     |
|-------------------|----------------|
|Ctrl-i or Alt-Right|Forward history.|
|Ctrl-o or Alt-Left |Back history.   |
|Ctrl-r             |Reload page.    |

#### Bar
|Key             |Description                                                                  |
|----------------|-----------------------------------------------------------------------------|
|Ctrl-:          |Focus to bar and start edit url or web search.                               |
|Ctrl-/          |Focus to bar and start in-site search.                                       |
|Ctrl-[ or escape|Escape from bar and close. Only working when bar has focus.                  |
|Ctrl-F          |Find next text. Only working when inputted in-site search query into bar.    |
|Ctrl-P          |Find previous text. Only working when inputted in-site search query into bar.|

#### Others
|Key   |Description               |
|------|--------------------------|
|Ctrl-N|Open new window.          |
|Ctrl-P|Open new incognito window.|
