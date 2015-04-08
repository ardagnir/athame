Athame
======

Athame is a patch for readline to add full Vim support by routing your keystrokes through an actual Vim process.

![Demo](http://i.imgur.com/MZCL1Vi.gif)

**Doesn't readline already come with a vi-mode?**

Yes, and if you're fine with a basic vi imitation designed by a bunch of Emacs users, feel free to use it. ...but for the crazy Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

**This is alpha-quality software. It has bugs. Use at your own risk.**

##Requirements
- Athame requires Vim (your version needs to have +clientserver).
- Athame works best in GNU/Linux.

##Installation
**Step 1:** Download the patched version of readline from this repo.

    git clone --recursive http://github.com/ardagnir/athame
    cd athame

*Note: If you want to patch readline yourself, you can run `git diff readline-6.3 HEAD` after this step to generate a patch*

**Step 2:** Build and install readline

    ./configure --prefix=/usr
    make SHLIB_LIBS=-lncurses
    sudo make install

*Note: Athame doesn't require ncurses, but most distros build readline with ncurses and a lot of software is built on this assumption.*

**Step 3:** Copy the default athamerc. This is optional but **strongly recommended.**

    cp athamerc ~/.athamerc

*Note: ~/.athamerc is like ~/.vimrc but for athame only. Add any athame-specific Vim customization here.*

##How to Use
Athame stores your Vim history in a Vim buffer with an empty line at the bottom and displays the line(s) of the cursor/highlighted text. Every key you type is sent to Vim and operates on this buffer. This allows you to use j/k/arrows to transverse history. Since this is Vim, you can use ? to search back and / to search forwards. The buffer is rebuilt from your new history after each command, so don't worry about destructive commands.

Unless you are using the Vim commandline(:,/,?), tabs and carriage returns are carried out by standard readline code.

##License

The source code is licensed under GPL v3. License is available [here](/LICENSE).
