if [ ! -d aconf ]; then
    mkdir aconf
fi
if [ ! -d m4 ]; then
    mkdir m4
fi

# libtoolize -c -f -i; sleep 1
echo "Running aclocal"
aclocal --force -Iaconf; sleep 1
echo "Running autoheader"
autoheader -f; sleep 1
echo "Running automake"
automake -a -c -f; sleep 1
echo "Running autoconf"
autoconf -f
