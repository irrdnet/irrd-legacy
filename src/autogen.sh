if [ ! -d m4 ]; then
    mkdir m4
fi

echo "Running aclocal"
aclocal
echo "Running autoconf"
autoconf
echo "Running automake"
automake --add-missing

