# Gets the data for all C experiments and puts it in the proper place so the benchmark script runs.
rm -rf datasets
wget https://www.spacetelescope.org/static/archives/images/publicationtiff40k/heic1502a.tif
mkdir -p datasets/
mv heic1502a.tif datasets/heic1502a-40k.tif 

