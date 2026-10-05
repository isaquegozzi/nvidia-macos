// test-slot.c — modelo userspace da máquina de estados do slot single-client.
//
// Espelha EXATAMENTE a lógica de GA106Lab::newUserClient/clientClosed/stop
// (reserva sob trava, libera em falha/close/died/stop). Valida que nenhum
// caminho deixa slot preso, retain vazado ou double-release. Lógica pura,
// sem kernel: prova o MODELO, não o runtime.
//
// Compilar: clang -o test-slot test-slot.c && ./test-slot
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#define RESERVED ((void *) 0x1)

typedef struct {
    pthread_mutex_t lock; /* IOSimpleLock no kernel (spin curto) */
    void *          active;
    int             ownerRetains; /* pareia retain em start / release em stop */
    int             stopCalls;
} Slot;

static void slotInit(Slot * s)
{
    pthread_mutex_init(&s->lock, NULL);
    s->active = NULL;
    s->ownerRetains = 0;
    s->stopCalls = 0;
}

/* Retorna: 0 ok / 1 root negado / 2 type inválido / 3 exclusivo / 4 falha etapa */
static int slotOpen(Slot * s, int isRoot, int type, int failAt, void ** out)
{
    void * client;
    if (!out) {
        return 5;
    }
    *out = NULL; /* como *handler = NULL na entrada do newUserClient real */
    if (!isRoot) {
        return 1;
    }
    if (type != 0) {
        return 2;
    }
    pthread_mutex_lock(&s->lock);
    if (s->active) {
        pthread_mutex_unlock(&s->lock);
        return 3;
    }
    s->active = RESERVED;
    pthread_mutex_unlock(&s->lock);
    client = (void *) 0x1000; /* OSTypeAlloc+init+attach+start ok */
    if (failAt) {             /* falha parcial em qualquer etapa */
        pthread_mutex_lock(&s->lock);
        if (s->active == RESERVED) {
            s->active = NULL;
        }
        pthread_mutex_unlock(&s->lock);
        return 4;
    }
    s->ownerRetains++; /* start() do client retém owner */
    pthread_mutex_lock(&s->lock);
    s->active = client;
    pthread_mutex_unlock(&s->lock);
    *out = client;
    return 0;
}

static void slotClientClosed(Slot * s, void * c)
{
    pthread_mutex_lock(&s->lock);
    if (c && s->active == c) {
        s->active = NULL;
    }
    pthread_mutex_unlock(&s->lock);
    /* + terminate() no kernel (idempotente no framework). */
}

static void slotStop(Slot * s)
{
    pthread_mutex_lock(&s->lock);
    s->active = NULL;
    pthread_mutex_unlock(&s->lock);
    if (s->ownerRetains > 0) {
        s->ownerRetains--; /* release NULL-guarded: uma única vez */
    }
    s->stopCalls++;
}

static void slotClientClose(Slot * s, void * c)
{
    slotClientClosed(s, c);
    /* terminate() → stop() do framework: */
    slotStop(s);
}

int main(void)
{
    Slot s;
    void *a = NULL, *b = NULL;
    int rc;

    /* 1. open A ok; open B concorrente → exclusivo; close A; open B ok. */
    slotInit(&s);
    assert(slotOpen(&s, 1, 0, 0, &a) == 0 && a);
    assert(slotOpen(&s, 1, 0, 0, &b) == 3 && !b);
    slotClientClose(&s, a);
    assert(slotOpen(&s, 1, 0, 0, &b) == 0 && b);
    assert(s.ownerRetains == 1);
    slotClientClose(&s, b);
    assert(s.active == NULL && s.ownerRetains == 0);

    /* 2. died sem close → vaga libera; próximo open ok. */
    slotInit(&s);
    assert(slotOpen(&s, 1, 0, 0, &a) == 0);
    slotClientClosed(&s, a); /* clientDied converge aqui */
    slotStop(&s);            /* teardown do framework */
    assert(slotOpen(&s, 1, 0, 0, &b) == 0 && b);
    slotClientClose(&s, b);

    /* 3. falha em cada etapa parcial nunca prende o slot. */
    slotInit(&s);
    assert(slotOpen(&s, 1, 0, 1, &a) == 4 && !a);
    assert(s.active == NULL);
    assert(slotOpen(&s, 1, 0, 0, &a) == 0 && a);
    slotClientClose(&s, a);

    /* 4. root/type negados antes de tocar no slot. */
    slotInit(&s);
    assert(slotOpen(&s, 0, 0, 0, &a) == 1 && !a);
    assert(slotOpen(&s, 1, 7, 0, &a) == 2 && !a);
    assert(s.active == NULL);

    /* 5. double close/died convergente: sem double-release. */
    slotInit(&s);
    assert(slotOpen(&s, 1, 0, 0, &a) == 0);
    slotClientClose(&s, a);
    slotClientClosed(&s, a); /* died tardio: idempotente */
    assert(s.active == NULL && s.ownerRetains == 0 && s.stopCalls == 1);

    /* 6. provider stop com cliente aberto: vaga limpa, sem dangling. */
    slotInit(&s);
    assert(slotOpen(&s, 1, 0, 0, &a) == 0);
    slotStop(&s);
    assert(s.active == NULL && s.ownerRetains == 0);
    assert(slotOpen(&s, 1, 0, 0, &b) == 0 && b);
    slotClientClose(&s, b);

    printf("test-slot: ALL PASS (6 cenários)\n");
    return 0;
}
