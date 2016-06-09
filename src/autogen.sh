if [ ! -d aconf ]; then
    mkdir aconf
fi
if [ ! -d m4 ]; then
    mkdir m4
fi

echo "Running aclocal"
aclocal
echo "Running autoheader"
autoheader
echo "Running autoconf"
autoconf
echo "Running automake"
automake --add-missing

