// GA106LabUserClientOwnerFlow.hpp — ONE_SHARED_USERCLIENT_OWNER_LIFETIME_FLOW.
//
// Política compartilhada de lifetime do owner no UserClient (PROMPT 57).
// Produção (UserClientOwnerRealOps, sobre GA106LabUserClient) e testes
// (UserClientOwnerMockOps) instanciam OS MESMOS templates. Static dispatch.
//
// Problema (Astra, 1.6.1): o handler lia fOwner como ponteiro cru e só
// depois chamava retain() — janela read->race->retain na qual stop() podia
// liberar/invalidar fOwner. Correção: o pin (load + retain) ocorre ATOMICAMENTE
// sob a mesma trava que protege o estado closing/open/fOwner. Não existe
// janela load-state -> unlock/race -> retain.
//
// Modelo (mínimo, sem redesign do IOKit):
//   lock(UserClient owner-state)
//     -> se closing/open==false/owner==NULL: reject (sem retain)
//     -> retain owner; active++; captura local
//   unlock
//     -> usa owner local (até ~4 s de poll GFW; sem travas retidas)
//   release em todos os exits; active-- sob trava.
//
// Lock order documentada (global, sem aninhamento):
//   UserClient fOwnerLock -> (sempre solta antes de qualquer outro lock)
//   Owner fSysmemLock (GFW/sysmem busy) -> seções curtas, nunca aninhada à UC.
// retain()/release() são refcount atômico único (OSObject via
// OSIncrementAtomic, libkern/OSAtomic.h — nunca dorme/bloqueia), logo seguro
// sob IOSimpleLock. Nenhum provider/MMIO/wait ocorre sob a trava UC.
// stop(): marca closing (impede novos pins) + drena active até 0 com espera
// curta fora da trava (in-flight é bounded pelo próprio selector).
// clientClose(): só marca closing (sem drain; stop drena).
// free(): trava e estado sobrevivem até o fim porque a trap de externalMethod
// retém o próprio UserClient; stop() já drenou active (assert documenta).
//
// Ops deve prover (só operações; policy AQUI):
//   lock(); unlock();
//   bool isClosing(); bool isOpen(); void *getOwnerRaw();
//   void markClosing(); void setOpen(bool);
//   void retainOwner(void *o); void releaseOwner(void *o);
//   int activeCount(); void setActiveCount(int);
//   void waitShort(); // quantum curto fora de lock (IODelay ~1ms / contado)
//   void onPinEntry(); // hook de sincronização de teste (no-op na produção)

#ifndef GA106LAB_USERCLIENT_OWNER_FLOW_HPP
#define GA106LAB_USERCLIENT_OWNER_FLOW_HPP

#include <stdint.h>

// Pin atômico do owner para externalMethod. Retorna true + owner retido em
// outOwner, ou false (rejeitado, sem retain, sem efeito colateral).
template <typename Ops>
inline bool PinOwnerForExternalMethod(Ops &ops, void *&outOwner)
{
    outOwner = NULL;
    ops.onPinEntry(); // hook de teste (barreira); no-op na produção.
    ops.lock();
    if (ops.isClosing() || !ops.isOpen() || ops.getOwnerRaw() == NULL) {
        ops.unlock();
        return false;
    }
    // Load + retain SOB a mesma trava que stop()/close() precisam para
    // invalidar: não existe janela load->race->retain (UC4).
    void *o = ops.getOwnerRaw();
    ops.retainOwner(o);
    ops.setActiveCount(ops.activeCount() + 1);
    ops.unlock();
    outOwner = o;
    return true;
}

// Liberação do pin: solta o retain e depois decrementa sob trava.
// Ordem: stop() só observa active==0 após o release do owner pelo chamador,
// logo stop nunca conclui enquanto o chamador ainda usa o owner local.
template <typename Ops>
inline void ReleasePinnedOwner(Ops &ops, void *owner)
{
    ops.releaseOwner(owner);
    ops.lock();
    ops.setActiveCount(ops.activeCount() - 1);
    ops.unlock();
}

// clientClose: marca closing de forma sincronizada (sem drain). Pins já
// obtidos seguem válidos (retain próprio); novos pins são rejeitados.
template <typename Ops>
inline void MarkUserClientClosingFlow(Ops &ops)
{
    ops.lock();
    ops.markClosing();
    ops.setOpen(false);
    ops.unlock();
}

// stop: impede novos pins e drena chamadas pinadas em andamento.
// Bounded porque toda chamada pinada é bounded (selector 8: contagem E
// deadline; demais selectors: imediatos). Sem espera sob trava.
template <typename Ops>
inline void BeginUserClientStopAndDrainFlow(Ops &ops)
{
    ops.lock();
    ops.markClosing();
    ops.setOpen(false);
    while (ops.activeCount() > 0) {
        ops.unlock();
        ops.waitShort();
        ops.lock();
    }
    ops.unlock();
}

#endif /* GA106LAB_USERCLIENT_OWNER_FLOW_HPP */
