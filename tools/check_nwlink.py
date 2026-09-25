with open(r'C:\Users\Utilisateur\AppData\Local\npm-cache\_npx\32401802e57ec35c\node_modules\nwlink\dist\index.js', encoding='utf-8', errors='ignore') as fp:
    f = fp.read()

pos = f.find('8006:')
print(f[pos:pos+1500])
