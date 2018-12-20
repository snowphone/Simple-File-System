

make

###############cpin
git checkout DISK*.img
expected=$(./sfs < test_cpin)

git checkout DISK*.img
actual=$(./mysfs < test_cpin)

git checkout DISK*.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "cpin test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi

###############cpin_full
git checkout DISK*.img
expected=$(./sfs < test_cpin_full)

git checkout DISK*.img
actual=$(./mysfs < test_cpin_full)

git checkout DISK*.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "cpin_full test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
###############cpout
git checkout DISK*.img
expected=$(./sfs < test_cpout)
rm -f ok12sfs

git checkout DISK*.img
actual=$(./mysfs < test_cpout)
rm -f ok12sfs

git checkout DISK*.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "cpout test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi

###############full_cmds
git checkout DISK*.img
expected=$(./sfs < test_full_cmds)

git checkout DISK*.img
actual=$(./mysfs < test_full_cmds)

git checkout DISK*.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "full_cmds test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi


