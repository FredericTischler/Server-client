Queuedir = SynchronizedQueue/
clientdir = Client/
launcherdir = CommandLauncher/
manualdir = Manuels/
CC = gcc
CFLAGS = -std=c18 \
  -Wall -D_POSIX_C_SOURCE=200809L -Wconversion -Werror -Wextra -Wpedantic -Wfatal-errors  -Wwrite-strings \
  -I$(Queuedir) -I$(launcherdir) -I$(clientdir) -O2
LDFLAGS = -lrt -lz -pthread

VPATH = $(Queuedir) $(launcherdir) $(clientdir)
objects_c = $(clientdir)Client.o  $(Queuedir)SynchronizedQueue.o
objects_s =  $(launcherdir)CommandLauncher.o $(Queuedir)SynchronizedQueue.o
executable_client = cmd
executable_launcher = launch
tar_archive = project_archive.tar.gz

all: $(executable_client) $(executable_launcher)

clean:
	$(RM) $(clientdir)Client.o $(launcherdir)CommandLauncher.o $(Queuedir)SynchronizedQueue.o $(executable_client) $(executable_launcher) $(tar_archive)

$(executable_client): $(objects_c)
	$(CC) $(objects_c) $(LDFLAGS) -o $(executable_client)

$(executable_launcher): $(objects_s)
	$(CC) $(objects_s) $(LDFLAGS) -o $(executable_launcher)

$(Queuedir)SynchronizedQueue.o: SynchronizedQueue.c SynchronizedQueue.h

$(clientdir)Client.o: Client.c SynchronizedQueue.h

$(launcherdir)CommandLauncher.o: CommandLauncher.c SynchronizedQueue.h

run_client: $(executable_client)
	./$(executable_client)

run_launcher: $(executable_launcher)
	./$(executable_launcher)

archive:
	tar -czvf $(tar_archive) $(Queuedir) $(clientdir) $(launcherdir) $(manualdir) Makefile

.PHONY: all clean run_client run_launcher archive
