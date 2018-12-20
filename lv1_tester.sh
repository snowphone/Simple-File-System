

make
git checkout DISK1.img
expected=$(./sfs < test_lscd)

git checkout DISK1.img
actual=$(./mysfs < test_lscd)

git checkout DISK1.img

actual=$(./mysfs < test_lscd)
diff <(echo "$expected") <(echo "$actual") 

if [ $? -eq '0' ]
then
	echo "lscd test: Success"
else
	echo "----------expected-----------"
	echo "$expected"
	echo "-----------actual------------"
	echo "$actual"
fi

