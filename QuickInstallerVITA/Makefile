TITLE_ID = QUICKINST
TARGET   = quickinstaller
OBJS     = main.o draw.o font.o bm.o utils.o file.o archive.o sha1.o \
		   minizip/zip.o minizip/unzip.o minizip/ioapi.o

LIBS = -lvita2d -lpng -ljpeg -lz -lm -lc \
	   -lSceAppMgr_stub -lSceAppUtil_stub -lSceCommonDialog_stub \
	   -lSceCtrl_stub -lSceDisplay_stub -lSceGxm_stub -lSceIme_stub \
	   -lSceHttp_stub -lSceKernel_stub -lSceNet_stub -lSceNetCtl_stub \
	   -lSceSsl_stub -lSceSysmodule_stub -lScePower_stub -lScePgf_stub libpromoter/libScePromoterUtil_stub.a

PREFIX   = arm-vita-eabi
CC       = $(PREFIX)-gcc
CXX      = $(PREFIX)-g++
CFLAGS   = -Wl,-q -Wall -O3 -Wno-unused-variable -Wno-unused-but-set-variable -std=c11
CXXFLAGS = $(CFLAGS) -std=c++11 -fno-rtti -fno-exceptions
ASFLAGS  = $(CFLAGS)

all: $(TARGET).vpk

%.vpk: eboot.bin
	vita-mksfoex -d PARENTAL_LEVEL=1 -s APP_VER=00.95 -s TITLE_ID=$(TITLE_ID) "$(TARGET)" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add pkg/sce_sys/icon0.png=sce_sys/icon0.png \
		--add pkg/sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add pkg/sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add pkg/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	$(TARGET).vpk
	
eboot.bin: $(TARGET).velf
	vita-make-fself $< $@

%.velf: %.elf
	vita-elf-create $< $@ $(VITASDK)/share/db.json

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.o: %.png
	$(PREFIX)-ld -r -b binary -o $@ $^
%.o: %.txt
	$(PREFIX)-ld -r -b binary -o $@ $^
%.o: %.bin
	$(PREFIX)-ld -r -b binary -o $@ $^

clean:
	@rm -rf $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) \
		eboot.bin param.sfo

vpksend: $(TARGET).vpk
	curl -T $(TARGET).vpk ftp://$(PSVITAIP):1337/ux0:/
	@echo "Sent."

send: eboot.bin
	curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/$(TITLE_ID)/
	@echo "Sent."
