mkdir -p ../sql.js/sqlite-src/sqlite-amalgamation/
cp build/shell.c ../sql.js/sqlite-src/sqlite-amalgamation
cp build/sqlite3.c ../sql.js/sqlite-src/sqlite-amalgamation
cp src/vdbeCompiler.cc ../sql.js/sqlite-src/sqlite-amalgamation
cp src/*.h ../sql.js/sqlite-src/sqlite-amalgamation
cp build/*.h ../sql.js/sqlite-src/sqlite-amalgamation