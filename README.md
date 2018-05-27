Athame
======

Athame patches your shell to add full Vim support by routing your keystrokes through an actual Vim process. Athame can currently be used to patch readline (used by bash, gdb, python, etc) and/or zsh (which doesn't use readline).

![Demo](https://i.imgur.com/74EoF4X.gif)

**Don't most shells already come with a vi-mode?**

Yes, and if you're fine with basic vi imitations designed by a bunch of Emacs users, feel free to use them. ...but for the true Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

## Requirements
- Athame works best on GNU/Linux.
- Athame also works on Windows (with WSL), and usually works on OSX. (I don't have a Mac, so I can't always test changes on one)
- Athame needs at least *one* of the following:
    - Vim 8.0+ with `+job` support
    - Vim 7.4+ with `+clientserver` support
      - Vim 7.4.928+ is strongly recommended.
      - The clientserver version requires X and will fall back to a normal shell if X isn't running.
    - Neovim 0.2.2+
      - Neovim support is currently experimental.
      - The setup scripts will look for vim by default. You can append `--vimbin=$(which nvim)` to have them use nvim.

## Download
Clone this repo recursively:

    git clone --recursive http://github.com/ardagnir/athame

## Setting up Athame Readline
### Option 1: (Arch Linux only) Use the AUR
Use `readline-athame-git` from the AUR.
 - If you are missing the readline gpg key, you can get it with `gpg --recv-keys BB5869F064EA74AB`

### Option 2: (The safest method) Install a local copy of Readline/Bash
    cd athame
    mkdir -p ~/local
    ./readline_athame_setup.sh --prefix=$HOME/local/
    ./bash_readline_setup.sh --prefix=$HOME/local/ --use_readline=$HOME/local/
    cp athamerc ~/.athamerc

You can now run ~/local/bin/bash to run bash with Athame.

### Option 3: Install Athame as your default Readline 
*For Ubuntu/Debian:*

    cd athame
    ./readline_athame_setup.sh --libdir=/lib/x86_64-linux-gnu --use_sudo

*Otherwise:*

    cd athame
    ./readline_athame_setup.sh --use_sudo

You may need to rebuild bash if your installed version doesn't use your system readline *(this is usually the case in Ubuntu and OSX)*:

    ./bash_readline_setup.sh --use_sudo

#### Additional Notes
- You can add the `--nobuild` flag to the setup script if you want to configure/build/install yourself.
- You can change what Vim binary is used by passing `--vimbin=/path/to/vim` to the setup script.

## Setting up Athame Zsh
### Option 1: (Arch Linux only) Use the AUR
Use `zsh-athame-git` from the AUR.
- If you are missing the zsh gpg key, you can get it with `gpg --recv-keys A71D9A9D4BDB27B3`
- Add "unset zle_bracketed_paste" to the end of your ~/.zshrc

### Option 2: (The safest method) Install a local copy of Zsh
*Windows Note: zsh tests don't work in Windows because zsh gets stuck in the background. Add `--notest` to the setup script line if installing on Windows.*

    cd athame
    mkdir -p ~/local
    ./zsh_athame_setup.sh --prefix=$HOME/local/
    cp athamerc ~/.athamerc

Add "unset zle_bracketed_paste" to the end of your ~/.zshrc

You can now run ~/local/bin/zsh to run zsh with Athame.

### Option 3: Install Athame as your default Zsh 
*Windows Note: zsh tests don't work in Windows because zsh gets stuck in the background. Add `--notest` to the setup script line if installing on Windows.*

    cd athame
    ./zsh_athame_setup.sh --use_sudo

Add "unset zle_bracketed_paste" to the end of your ~/.zshrc

#### Additional Notes
- You can add the `--nobuild` flag to the setup script if you want to configure/build/install yourself.
- You can change what Vim binary is used by passing `--vimbin=/path/to/vim` to the setup script.


## Configuration
Athame can be configured through the following environment variables. They can be set on the fly or you can add them to your ~/.bashrc or ~/.zshrc. Make sure you use `export` if you add them to your ~/.zshrc.

- **ATHAME_ENABLED:** Set to 0 to disable Athame.
- **ATHAME_SHOW_MODE:** Set to 0 to hide mode display at the bottom of the screen. To display it yourself, read the ATHAME_VIM_MODE variable.
- **ATHAME_SHOW_COMMAND:** Set to 0 to hide :, /, and ? commands at the bottom of the screen. To display them yourself, read the ATHAME_VIM_COMMAND variable.
- **ATHAME_SHOW_ERROR:** Set to 0 to hide athame errors. If you want to display these yourself, read the ATHAME_ERROR variable.
- **ATHAME_VIM_PERSIST:** Normally Athame spawns a new short-lived Vim instance for each readline/zle usage (in practice, a separate Vim instance each line). If you set this to 1, you will get 1 Vim instance per shell instead, so your iabbrevs, etc, are preserved from line to line. The tradeoff is that it will keep Vim open until the shell closes, whether or not it is using readline. For example: if you open your terminal and type bash 10 times, with the default ATHAME_VIM_PERSIST of 0 you will have one instance of Vim running, because only the inner shell is using readline, but with ATHAME_VIM_PERSIST set to 1, you will have 10.
- **ATHAME_USE_JOBS:** If non-zero, Athame will use jobs for async behavior. Otherwise, it will use clientserver.

## FAQ
#### How do I use this?
It's just like Vim. Imagine your history is stored inside a Vim buffer (because it is!) with a blank line at the bottom. Your cursor starts on that blank line each time readline is called.
Unless you're in command mode, some special chars (such as carriage return) are handled by readline/zsh.

Some example commands (there's no specific code for these, it's just Vim):

- j: go back a line in history
- k: go forward a line in history
- ?: search history (and current line) backwards (better if you have :set incsearch enabled)
- /: search history (and current line) forwards (better if you have :set incsearch enabled)
- cc: clear current line and go to insert mode

The default athamerc modifies up/down arrows in insert mode to jump to the end of the line, like in a normal shell, but you can change this.

You can also enable history searching arrows in the athamerc. (If you enable this and you use arrows, it searches for text that begins with what you've typed. It's like Vim's command mode.)


#### ? and / are reversed from bash's vi-mode. Why?
Bash's vi-mode has ? go down and / go up. In actual Vim (and thus, Athame) they go the other way.

#### Why does Athame start in insert/normal mode?
The default athamerc includes "startinsert" to make Athame start in insert mode. If you remove that line, it will start in normal mode.

#### Why is my Athame slow?
Athame should be very fast, but it will slow down if your Vim setup is slow or if you have invalid vimscript in either your athamerc or vimrc. Try using a clean vimrc or athamerc and see if it speeds up. If not, file an issue.

#### I installed Athame for Readline, but it isn't doing anything!
Are you using the system readline?
Type:

    ldd $(which bash)
This should include libreadline. If it doesn't, you need to build bash to use the system readline. You can do this by running:

    ./bash_readline_setup.sh

#### Which chars are sent to vim and which to readline/zsh?

Control-C usually sends a SIGNINT signal that is handled by zsh, readline, or the program that calls readline. For other keys:

- In readline, chars marked as rl_delete (usually Control-D), rl_newline (usually return), rl_complete (usually tab), and rl_clear_screen (usually Control-L) are sent to readline. All other keys are sent to vim. This means that if you use readline's built-in vi-mode, Control-L and Control-D probably won't be sent to the shell.
- In zsh, Athame is hardcoded to send tab, Control-D, carriage return, new line, and Control-L to zsh. All other keys are sent to vim. This behavior will likely change in the future.

#### I got the error "Couldn't load vim path"
Is Vim at the correct path? You can change the path Athame looks at with the --vimbin flag.

#### How do I disable/uninstall Athame?
To temporarily disable Athame, `export ATHAME_ENABLED=0`

To compile readline/zsh without the Athame patches, run the setup script with a `--noathame` flag.

To get rid of Athame completly, you should probably just replace it with the non-patched version of readline/zsh from your distro.

Or, if you're cool with not having a shell, you can uninstall it by cding into the readline or zsh directory and typing `sudo make uninstall`.

Depending on your approach, you may want to manually remove `/usr/lib/athame*` and `/etc/athamerc` as well.

#### Why do the Up/Down arrows make the cursor jump to the end of the line?
This is done through vimscript in the athamerc to make Athame more like a normal shell. Feel free to comment it out.

#### How do I disable/change the way Athame shows Vim's mode?
By default Athame shows the Vim mode at the bottom of the screen. This can be disabled using `export ATHAME_SHOW_MODE=0`

Athame stores the current mode in the `ATHAME_VIM_MODE` environment variable. You can use this to display the Vim mode yourself. See https://github.com/ardagnir/athame/issues/21 for an example using powerline.

Similarly, Vim commands are displayed at the bottom of the screen by default. These can be disabled using `export ATHAME_SHOW_COMMAND=0` and accessed using `ATHAME_VIM_COMMAND`

#### Why don't arrow keys work when I add Athame to my convoluted Zsh setup?
Because arrow keys are evil! Just kidding. :P

Ohmyzsh and some other zsh setups put your terminal into application mode to help their keymaps. Athame doesn't like this, and besides, you're using Vim as your keymap. You can override this by adding the following to the end of your ~/.zshrc:

    function zle-line-init(){
      echoti rmkx
    }
    zle -N zle-line-init

#### Why isn't there an Athame package for my favorite distro?
...because you haven't made one yet. The Athame setup script comes with a --nobuild flag so that you can build it however you want or your package can just apply the Athame patches itself.

#### This is awesome! Can I help?
The best way to help is to look at the issue section and submit patches to fix bugs.

If you have a shell that I'm missing, you can also try making a patch to communicate between it and Athame (see athame_readline.h and athame_zsh.h for the functions you need to implement).

#### What about donations?
I'm not accepting donations, but you should consider donating to the [EFF](https://supporters.eff.org/donate/) so that we don't end up living in a scary distopian future where everyone is forced to use emacs.


## Bugs
- See [issues](https://github.com/ardagnir/athame/issues)
- If you see a bug without an issue, please create one.

## License
GPL v3
