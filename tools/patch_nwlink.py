p = r'C:\Users\Utilisateur\AppData\Local\npm-cache\_npx\32401802e57ec35c\node_modules\nwlink\dist\index.js'
with open(p, 'r', encoding='utf-8') as f:
    content = f.read()

target = 'return n=e.infos.firmware,'
if target in content:
    replacement = 'console.log("FIRMWARE_INFO:", JSON.stringify(e.infos.firmware)), return n=e.infos.firmware,'
    # wait, comma operator in JS
    replacement = 'console.log("FIRMWARE_INFO:", JSON.stringify(e.infos.firmware)), n=e.infos.firmware,'
    content = content.replace(target, 'n=e.infos.firmware, console.log("FIRMWARE_INFO:", JSON.stringify(e.infos.firmware)), ')
    with open(p, 'w', encoding='utf-8') as f:
        f.write(content)
    print('Successfully patched nwlink!')
else:
    print('Target not found')
