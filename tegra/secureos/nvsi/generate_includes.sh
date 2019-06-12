rm -rf include/*
for file in ./xml/*.xml; do
	if [ -f "$file" ]; then
                filename=$(basename $file)
		xxd -i $file include/$filename.h
		sed -i '/unsigned int/d' include/$filename.h
		sed -i '1 s/^.*$/unsigned char nvsi_xml_data[] = {/g' include/$filename.h
	fi
done
