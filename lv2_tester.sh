

make
git checkout DISK1.img
expected=$(./sfs < test_touch)

git checkout DISK1.img
actual=$(./mysfs < test_touch)

diff <(echo "$expected") <(echo "$actual") 

git checkout DISK1.img

if [ $? -eq '0' ]
then
	echo "touch test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi
