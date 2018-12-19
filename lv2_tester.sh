

make

###############touch
git checkout DISK1.img
expected=$(./sfs < test_touch)

git checkout DISK1.img
actual=$(./mysfs < test_touch)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "touch test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi

###############mkdir
git checkout DISK1.img
expected=$(./sfs < test_mkdir)

git checkout DISK1.img
actual=$(./mysfs < test_mkdir)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "mkdir test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
###############rmdir
git checkout DISK1.img
expected=$(./sfs < test_rmdir)

git checkout DISK1.img
actual=$(./mysfs < test_rmdir)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "rmdir test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi

###############mv
git checkout DISK1.img
expected=$(./sfs < test_mv)

git checkout DISK1.img
actual=$(./mysfs < test_mv)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "mv test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
###############rm
git checkout DISK1.img
expected=$(./sfs < test_rm)

git checkout DISK1.img
actual=$(./mysfs < test_rm)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "rm test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
###############stress_dir
git checkout DISK1.img
expected=$(./sfs < test_stress_dir)

git checkout DISK1.img
actual=$(./mysfs < test_stress_dir)

git checkout DISK1.img

diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "stress_dir test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
