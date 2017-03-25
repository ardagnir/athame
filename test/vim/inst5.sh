export ATHAME_VIM_PERSIST=0
echo xx > out5
:inoremap xx mapped
iecho xx >> out5
echo xx >> out5
export ATHAME_VIM_PERSIST=1
echo xx >> out5
:inoremap xx mapped
iecho xx >> out5
echo xx >> out5
echo xx xx x >> out5
export ATHAME_VIM_PERSIST=0
# The env isn't set until after readline ends, so don't check on next line.
echo z >> out5
echo xx >> out5
