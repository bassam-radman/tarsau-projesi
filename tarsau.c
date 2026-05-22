#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
    Windows/MSYS2 ortamında metin modu satır sonlarını değiştirebilir.
    Bu yüzden bütün dosyalar binary modda açılır.
*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef _WIN32
#define MKDIR(path) mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777)
#endif

#define MAX_DOSYA 32
#define MAX_TOPLAM_BOYUT (200LL * 1024LL * 1024LL)
#define BUFFER_SIZE 4096

typedef struct {
    char ad[256];
    int izin;
    long long boyut;
} DosyaBilgi;

/*
    Dosyanın ASCII metin dosyası olup olmadığını kontrol eder.
*/
int ascii_metin_mi(const char *dosya_adi) {
    int fd = open(dosya_adi, O_RDONLY | O_BINARY);
    if (fd < 0) return 0;

    unsigned char c;
    ssize_t okunan;

    while ((okunan = read(fd, &c, 1)) == 1) {
        if (!(c == 9 || c == 10 || c == 13 || (c >= 32 && c <= 126))) {
            close(fd);
            return 0;
        }
    }

    close(fd);

    return okunan == 0;
}

/*
    write() her zaman bütün veriyi tek seferde yazmayabilir.
    Bu fonksiyon tüm verinin yazılmasını garanti eder.
*/
int yaz_tam(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;

    while (n > 0) {
        ssize_t yazilan = write(fd, p, n);

        if (yazilan <= 0) {
            return -1;
        }

        p += yazilan;
        n -= yazilan;
    }

    return 0;
}

/*
    Verilen yoldan sadece dosya adını alır.
    Örnek: klasor/t1.txt -> t1.txt
*/
const char *sadece_ad(const char *yol) {
    const char *p1 = strrchr(yol, '/');
    const char *p2 = strrchr(yol, '\\');

    if (p1 && p2) return p1 > p2 ? p1 + 1 : p2 + 1;
    if (p1) return p1 + 1;
    if (p2) return p2 + 1;

    return yol;
}

/*
    -b modu: dosyaları .sau arşivine birleştirir.
*/
int arsivle(int argc, char *argv[]) {
    char *arsiv_adi = "a.sau";
    int o_index = argc;

    /*
        -o parametresi varsa arşiv adını al.
    */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                printf("-o parametresinden sonra dosya adi yok!\n");
                return 1;
            }

            arsiv_adi = argv[i + 1];
            o_index = i;
            break;
        }
    }

    int dosya_sayisi = 0;
    long long toplam_boyut = 0;

    /*
        Giriş dosyalarını kontrol et.
    */
    for (int i = 2; i < o_index; i++) {
        struct stat st;

        if (stat(argv[i], &st) != 0 || !S_ISREG(st.st_mode)) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", argv[i]);
            return 1;
        }

        if (!ascii_metin_mi(argv[i])) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", argv[i]);
            return 1;
        }

        dosya_sayisi++;
        toplam_boyut += (long long)st.st_size;

        if (dosya_sayisi > MAX_DOSYA) {
            printf("En fazla 32 giris dosyasi verilebilir!\n");
            return 1;
        }

        if (toplam_boyut > MAX_TOPLAM_BOYUT) {
            printf("Toplam boyut 200 MB sinirini asiyor!\n");
            return 1;
        }
    }

    if (dosya_sayisi == 0) {
        printf("Arsivlenecek dosya yok!\n");
        return 1;
    }

    /*
        Header oluştur.
        Format:
        |dosyaadi,izin,boyut||dosyaadi,izin,boyut|
    */
    char header[8192] = "";
    char kayit[512];

    for (int i = 2; i < o_index; i++) {
        struct stat st;

        if (stat(argv[i], &st) != 0) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", argv[i]);
            return 1;
        }

        snprintf(
            kayit,
            sizeof(kayit),
            "|%s,%04o,%lld|",
            sadece_ad(argv[i]),
            st.st_mode & 0777,
            (long long)st.st_size
        );

        if (strlen(header) + strlen(kayit) >= sizeof(header)) {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            return 1;
        }

        strcat(header, kayit);
    }

    /*
        İlk 10 byte header uzunluğunu tutar.
    */
    char ilk10[11];
    snprintf(ilk10, sizeof(ilk10), "%010d", (int)strlen(header));

    int out = open(arsiv_adi, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);

    if (out < 0) {
        printf("Arsiv dosyasi olusturulamadi!\n");
        return 1;
    }

    if (yaz_tam(out, ilk10, 10) < 0 || yaz_tam(out, header, strlen(header)) < 0) {
        printf("Arsiv dosyasi olusturulamadi!\n");
        close(out);
        return 1;
    }

    /*
        Dosya içeriklerini header'dan sonra ekle.
    */
    char buffer[BUFFER_SIZE];

    for (int i = 2; i < o_index; i++) {
        int in = open(argv[i], O_RDONLY | O_BINARY);

        if (in < 0) {
            printf("%s dosyasi acilamadi!\n", argv[i]);
            close(out);
            return 1;
        }

        ssize_t okunan;

        while ((okunan = read(in, buffer, sizeof(buffer))) > 0) {
            if (yaz_tam(out, buffer, okunan) < 0) {
                printf("Arsiv dosyasi olusturulamadi!\n");
                close(in);
                close(out);
                return 1;
            }
        }

        close(in);
    }

    close(out);

    printf("Dosyalar birlestirildi.\n");
    return 0;
}

/*
    -a modu: .sau arşivinden dosyaları çıkarır.
*/
int arsivden_cikar(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        printf("Kullanim: ./tarsau -a arsiv.sau [hedef_dizin]\n");
        return 1;
    }

    char *arsiv_adi = argv[2];
    const char *hedef_dizin = argc == 4 ? argv[3] : ".";

    int len = (int)strlen(arsiv_adi);

    if (len < 5 || strcmp(arsiv_adi + len - 4, ".sau") != 0) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    int in = open(arsiv_adi, O_RDONLY | O_BINARY);

    if (in < 0) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    /*
        İlk 10 byte header uzunluğu.
    */
    char ilk10[11];

    if (read(in, ilk10, 10) != 10) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        close(in);
        return 1;
    }

    ilk10[10] = '\0';

    for (int i = 0; i < 10; i++) {
        if (ilk10[i] < '0' || ilk10[i] > '9') {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            close(in);
            return 1;
        }
    }

    int header_uzunluk = atoi(ilk10);

    if (header_uzunluk <= 0 || header_uzunluk > 8191) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        close(in);
        return 1;
    }

    char *header = (char *)malloc((size_t)header_uzunluk + 1);

    if (header == NULL) {
        printf("Bellek hatasi!\n");
        close(in);
        return 1;
    }

    if (read(in, header, (size_t)header_uzunluk) != header_uzunluk) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(header);
        close(in);
        return 1;
    }

    header[header_uzunluk] = '\0';

    DosyaBilgi dosyalar[MAX_DOSYA];
    int dosya_sayisi = 0;

    /*
        Header parse.
        Header'da kayıtlar | ile ayrılır.
        İki kayıt arasında || olabilir, bu yüzden boş kayıtlar atlanır.
    */
    char *p = header;

    while (*p != '\0') {
        while (*p == '|') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        char *son = strchr(p, '|');

        if (son == NULL) {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(header);
            close(in);
            return 1;
        }

        *son = '\0';

        char izin_str[16];

        if (dosya_sayisi >= MAX_DOSYA) {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(header);
            close(in);
            return 1;
        }

        if (sscanf(
                p,
                "%255[^,],%15[^,],%lld",
                dosyalar[dosya_sayisi].ad,
                izin_str,
                &dosyalar[dosya_sayisi].boyut
            ) != 3) {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(header);
            close(in);
            return 1;
        }

        dosyalar[dosya_sayisi].izin = (int)strtol(izin_str, NULL, 8);

        if (dosyalar[dosya_sayisi].boyut < 0) {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(header);
            close(in);
            return 1;
        }

        dosya_sayisi++;
        p = son + 1;
    }

    struct stat st;

    if (stat(hedef_dizin, &st) != 0) {
        if (MKDIR(hedef_dizin) != 0) {
            printf("Hedef dizin olusturulamadi!\n");
            free(header);
            close(in);
            return 1;
        }
    }

    /*
        Dosya içeriklerini sırayla çıkar.
    */
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < dosya_sayisi; i++) {
        char hedef_yol[512];

        snprintf(
            hedef_yol,
            sizeof(hedef_yol),
            "%s/%s",
            hedef_dizin,
            dosyalar[i].ad
        );

        int out = open(
            hedef_yol,
            O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
            dosyalar[i].izin
        );

        if (out < 0) {
            printf("Dosya olusturulamadi: %s\n", hedef_yol);
            free(header);
            close(in);
            return 1;
        }

        long long kalan = dosyalar[i].boyut;

        while (kalan > 0) {
            int okunacak = kalan > BUFFER_SIZE ? BUFFER_SIZE : (int)kalan;
            ssize_t okunan = read(in, buffer, okunacak);

            if (okunan <= 0) {
                printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
                close(out);
                free(header);
                close(in);
                return 1;
            }

            if (yaz_tam(out, buffer, (size_t)okunan) < 0) {
                printf("Dosya yazilamadi!\n");
                close(out);
                free(header);
                close(in);
                return 1;
            }

            kalan -= okunan;
        }

        close(out);
        chmod(hedef_yol, dosyalar[i].izin);
    }

    printf("%s dizininde dosyalar acildi.\n", hedef_dizin);

    free(header);
    close(in);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Kullanim:\n");
        printf("./tarsau -b dosyalar -o arsiv.sau\n");
        printf("./tarsau -a arsiv.sau [hedef_dizin]\n");
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        return arsivle(argc, argv);
    }

    if (strcmp(argv[1], "-a") == 0) {
        return arsivden_cikar(argc, argv);
    }

    printf("Gecersiz parametre!\n");
    return 1;
}
