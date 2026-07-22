# 测试证书的签名
. "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest .\MetasequoiaImeServer.manifest -outputresource:.\build\bin\Debug\MetasequoiaImeServer.exe; 1
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /fd SHA256 /v /s PrivateCertStore /n "Test Certificate - For Internal scitertest Use Only" /a .\build64\Debug\MetasequoiaImeTsf.dll

# 真实证书的签名，使用时只需把 Thumbprint 替换为真实证书的 Thumbprint 即可
## Debug
### 32 bit
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build32\Debug\MetasequoiaImeTsf.dll
### 64 bit
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build64\Debug\MetasequoiaImeTsf.dll

## Release
### 32 bit
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build32-release\Release\MetasequoiaImeTsf.dll
### 64 bit
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build64-release\Release\MetasequoiaImeTsf.dll