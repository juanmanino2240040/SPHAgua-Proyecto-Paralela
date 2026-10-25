PATH=random4
echo "n,steps,t,clock" > $PATH 
for VARIABLE in 10 50 100 250 500 750 1000 3000 5000 7000 9000 10000
do
    ./build/bin/main $VARIABLE 1000 $PATH 1.00005
done