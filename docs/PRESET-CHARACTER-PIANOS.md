# Preset "Character Pianos": come sono stati fatti

I 16 preset della pagina MODIFICA imitano il colore delle Performance "Character Pianos" di Yamaha Montage e MODX.
Il punto di partenza è sempre il Maestro Concert Grand. Nel plugin non c'è nessun campione Yamaha: la banca Character Pianos
è un contenuto Yamaha con la sua licenza e non si può estrarre né ridistribuire. Si imita il suono, non si copia.

## Da dove vengono le misure

Il riferimento è il video dimostrativo che Mirco ha mandato, "Yamaha Montage | Free sound bank Character Pianos Audio Demo"
del canale Moessieurs (5 agosto 2023, https://www.youtube.com/watch?v=PU39f8vyYMA). Tra 3:58 e 11:34 suona le 16 Performance
una dopo l'altra, senza parlare sopra. I confini di ogni Performance sono stati presi leggendo il nome sullo schermo del Montage,
un fotogramma al secondo (OCR con Tesseract). Il video non è nel repository.

Il tempo che il Montage mostra in alto è 89 BPM per quasi tutte, 125 per Felt Piano ARP*4 e 86 per C3 Ballad ARP: i due preset
con l'arpeggio usano questi tempi.

## Il metodo

1. Per ogni Performance: spettro medio in ottave da 63 Hz a 8 kHz (rispetto a 1 kHz), differenza tra i canali (larghezza stereo),
   dinamica. L'audio del video è AAC a 128 kbit/s e sopra gli 8 kHz è poco affidabile: quella banda pesa la metà.
2. Le note suonate nel video cambiano da una Performance all'altra, e lo spettro dipende molto dal registro. Per confrontare il
   Maestro con il Montage alla pari si stimano le note usate in ogni pezzo (salienza armonica sullo spettro), si scrive un MIDI con
   accordi presi da quelle note e si fa suonare il Maestro, poi lo si passa per lo stesso AAC a 128 kbit/s.
3. Taratura: il preset viene esportato dal plugin (stesso codice degli export) su quel MIDI, misurato nello stesso modo e corretto
   (EQ a 4 bande con un modello dei filtri, larghezza dalla differenza tra i canali), per tre giri. Per i preset con il feltro
   si provano diverse quantità di feltro e di fruscio e si tiene la migliore.
4. Le correzioni dell'EQ sono limitate a ±10 dB: le misure hanno un margine di qualche dB e una correzione estrema non vale il rischio.
5. Livello: ogni preset è esportato sui primi 12 secondi di "Riky's Theme" e il controllo LEVEL lo porta allo stesso volume del
   Maestro, entro mezzo decibel. Così cambiare preset non fa saltare il volume.

Script e numeri sono in `tools/calibration`: `analyze.py` misura, `notes.py` stima le note e scrive i MIDI di riferimento,
`calibrate.py` tara, `trims.py` uguaglia i livelli, `genpresets.py` scrive la tabella in `Source/Presets.cpp`.

## Quanto ci si avvicina

Errore medio sulle 8 bande (dB, pesato), prima con il Maestro così com'è e dopo la taratura:

| Preset | Maestro com'è | Dopo |
|---|---|---|
| Nashville C3 | 3,2 | 1,2 |
| U1 Upright | 7,3 | 1,9 |
| Felt Piano | 7,2 | 3,1 |
| Nashville C3 + DX EP | 4,2 | 2,2 |
| Nashville C3 Bright | 6,0 | 2,7 |
| U1 Upright Bright | 2,9 | 1,7 |
| Felt Piano Bright | 2,2 | 1,5 |
| Felt Mono-SP Piano | 5,2 | 2,3 |
| Nashville C3 + SynPad | 5,3 | 1,9 |
| U1 Upright + Rd EP | 2,0 | 1,1 |
| Felt Piano Pad | 10,0 | 5,3 |
| U1 Upright + Felt | 3,6 | 1,4 |
| NashC3 + SynFMBrass | 4,7 | 1,7 |
| NashC3 + Felt Pad | 5,6 | 1,7 |
| Felt Piano ARP*4 | 6,0 | 2,9 |
| C3 Ballad ARP | non misurato | |

C3 Ballad ARP non è stato tarato sul video: nella Performance suonano anche batteria e basso, che falsano lo spettro.
Usa il Nashville C3 con un pad e l'arpeggio in ottavi solo sul pianoforte.

## Cosa non si può imitare così

- L'EQ cambia il colore medio, non il comportamento della singola nota: un verticale vero ha un attacco e una risonanza diversi da
  un gran coda, e questo resta del Maestro.
- Felt Piano Pad resta il più lontano (5 dB): il pad della Performance riempie i bassi più di quello di Riky Piano Man.
- Le misure vengono da un solo video compresso. Con registrazioni pulite dal Montage (note singole Do2–Do6 a tre dinamiche,
  un accordo col pedale, trenta secondi di musica) si possono rifare con lo stesso metodo e andare più vicino.

## Il secondo strato

I suoni del secondo strato (piano elettrico FM, piano elettrico a lamelle, pad caldo, pad sintetico, ottoni FM, pad di feltro,
basso sintetico) sono strumenti SFZ originali, scritti per Riky Piano Man con gli oscillatori di sfizz: nessun campione di terzi.
"Pianoforte con feltro" è il Maestro stesso con il feltro al 70 %. A volume 0 dB ogni suono è forte quanto il Maestro su
"Riky's Theme"; i preset li mettono da 4 a 11 dB sotto il pianoforte.
