#pragma once
enum Op { Bind, LoginRoot, DestroyRoot, ValueActive, ValueInactive, Tick, SessionTick,
          ConnectedB, GameClick, AccountClick, WriteA, WriteB, WriteOther,
          CryptoWriteA, CryptoWriteOther, ReadA, ReadB, ReadOther,
          CryptoReadA, CryptoReadOther, AvailableA, AvailableOther,
          SetDialogFailure, SetDisconnected, EnterGameplay, NoSession, SessionRestored,
          SetFastFailureException, ClearFastFailureException, ClearDialogFailure, SlowSocketClose, SocketFlagA, SocketFlagB, AsyncConnectB };
enum Counter { Writes, Reads, CryptoWrites, CryptoReads, AvailableCalls, Clicks,
               AccountClicks, Dialogs, RootNetworkPassed, BindCalls, TickCalls,
               ReconnectFailures, CorrectFailureArgs, MainThreadFailures, SocketCloses, WorkerSocketCloses, AsyncConnects, CleanAsyncConnects };
