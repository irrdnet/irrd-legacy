echo "Running aclocal"
aclocal
echo "Running autoconf"
autoconf
echo "Running automake"
automake --add-missing

