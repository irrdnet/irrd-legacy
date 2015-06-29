sed -i.bak "s/@@date@@/$(date +%Y%m%d)/" configure.ac
mkdir m4
echo "Running aclocal"
aclocal
echo "Running autoconf"
autoconf
echo "Running automake"
automake --add-missing

