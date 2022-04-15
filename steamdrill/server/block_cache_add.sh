export LD_LIBRARY_PATH="../lib/elfutils/install/lib/;../lib/capstone"



export pthread=$OMNIPLAY_DIR/eglibc-2.15/prefix/lib
export replay="$OMNIPLAY_DIR/test/resume --pthread $pthread"


echo "adding blocks..."
../static_tables/mem_maps $1 | awk '
{
	split($2, chars, "");
        for (i = 1; i <= length($2); ++i) {
	    if (chars[i] > "!" && chars[i] < "~")
	       printf("%s", chars[i]);
	}
	printf("\n")
 }' |  xargs  -I{} ./get_blocks {} block_cache/
echo "done."


#echo "adding partitions..."
for i in {0..6}; do
    ./add_partition $1 $((2**i));
#    vals=`./check_partition $1 $((2**i)) | awk '$5 >0 {print $5}'`
#    for v in $vals; do
#	if [ ! -e "$1/ckpt.$v" ]; then#
#	    $replay $1 --ckpt_at=$v#
#	fi
#    done
done
