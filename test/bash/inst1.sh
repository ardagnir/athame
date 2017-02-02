input=0
read -e input
good
echo $input > out1
read -en3 input
yayecho $input >> out1
