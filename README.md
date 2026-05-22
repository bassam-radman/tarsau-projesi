# TARSAU Projesi

Bu proje Sistem Programlama dersi kapsamında geliştirilmiştir.

## Özellikler

* ASCII metin dosyalarını arşivleme
* .sau formatında arşiv oluşturma
* Arşivden dosya çıkarma
* Dosya izinlerini koruma
* Makefile ile derleme

## Derleme

```bash
make
```

## Kullanım

Arşiv oluşturma:

```bash
./tarsau -b dosya1 dosya2 -o arsiv.sau
```

Arşiv çıkarma:

```bash
./tarsau -a arsiv.sau hedef_klasor
```
