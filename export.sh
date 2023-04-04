mkdir -p ../sql.js/sqlite-src/sqlite-amalgamation/
cp $@/shell.c ../sql.js/sqlite-src/sqlite-amalgamation
cp $@/sqlite3.c ../sql.js/sqlite-src/sqlite-amalgamation
cp src/vdbeCompiler.cc ../sql.js/sqlite-src/sqlite-amalgamation
cp src/*.h ../sql.js/sqlite-src/sqlite-amalgamation
cp $@/*.h ../sql.js/sqlite-src/sqlite-amalgamation