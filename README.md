Athame
======

Athame is a patchset for readline to add full vim support by routing your keystrokes through an actual vim process.

*Doesn't readline already come with a vi-mode?*

Yes, and if you're fine with a basic vi imitation designed by a bunch of Emacs users, feel free to use it. ...but for the crazy Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

**Athame is not stable. Do not use this on a production system.**


##Requirements
- Athame requires Vim (your version needs to have +clientserver).
- Athame works best in GNU/Linux.

##Installation
**Step 1:** If you don't have the [vimbed](https://github.com/ardagnir/vimbed) plugin, install it first using your plugin-manager. If you use pathogen:

    cd ~/.vim/bundle
    git clone http://github.com/ardagnir/vimbed

**Step 2:** Download the patched version of readline from this repo. (If you want to patch readline yourself, you can run `git diff readline-6.3 HEAD` to generate a patch)

**Step 3:** From inside the patched readline source:

    ./configure
    make

**Step 4:** You can now install, but **remember that athame might break things, including bash**:

    sudo make install

##Licesnse

GPL v3
