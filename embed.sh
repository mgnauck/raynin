#!/bin/bash

infile_with_embedding_markers=$1
begin_embedding_marker=$2
end_embedding_marker=$3
infile_with_embedding_content=$4
outfile=$5

# Embeds a multi-line string contained in one file into another file. The embedding area is delimited by begin and end marker.
# Args: infile_with_embedding_markers begin_embedding_marker end_embedding_marker infile_with_embedding_content outfile
function embeddingInFile() {
  echo -e "/$2/r $4\n/$2/,/$2/+1j\n/$3/-1,/$3/j\ns/$3//g\n/$2/s///g\nw $5\nq" | ed $1 > /dev/null
}

#echo "infile with embedding markers:" $infile_with_embedding_markers
#echo "begin embedding marker:" $begin_embedding_marker
#echo "end embedding marker:" $end_embedding_marker
#echo "infile with embedding content:" $infile_with_embedding_content
#echo "outfile:" $outfile

embeddingInFile $infile_with_embedding_markers $begin_embedding_marker $end_embedding_marker $infile_with_embedding_content $outfile
