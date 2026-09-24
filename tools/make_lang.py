#!/usr/bin/env python3
# Writes resources/lang/<Language>.txt - the Terminal App's own strings in the 16 languages AutoBleem has, the
# Key=Value format the launcher reads (Lang::loadMore lays them over the main GUI's file). A string the main
# GUI's file already has (Delete, Space, Enter, Keyboard) is left out: the lookup falls through to it.
# Every string on screen is here in every language, translated - edit the table and run this again.
import os

KEYS = [
    "Terminal",
    "Cannot start the shell:",
    "The shell has ended.",
    "Press any button to close.",
    "Lines back:",
    "Close",
    "Type",
    "Hide keyboard",
    "Show keyboard",
    "Menu",
    "Shift/Ctrl",
    "Scroll",
    "Page Up/Down",
    "The on-screen keyboard for the pad",
    "Larger text",
    "Fewer, bigger characters",
    "Smaller text",
    "More rows and columns",
    "Leave full screen",
    "Full screen",
    "The terminal over the whole screen",
    "Send Ctrl+C",
    "Interrupt the running program",
    "Send Ctrl+D",
    "End of input - at an empty prompt, ends the shell",
    "Send Ctrl+Z",
    "Suspend the running program",
    "Close the terminal",
    "Ends the shell and every program started from it",
]

T = {
    "Polski": [
        "Terminal", "Nie można uruchomić powłoki:", "Powłoka zakończyła działanie.",
        "Naciśnij dowolny przycisk, aby zamknąć.", "Wierszy wstecz:", "Zamknij", "Pisz", "Ukryj klawiaturę",
        "Pokaż klawiaturę", "Menu", "Shift/Ctrl", "Przewijanie", "Strona w górę/w dół",
        "Klawiatura ekranowa do obsługi padem", "Większy tekst", "Mniej, ale większych znaków", "Mniejszy tekst",
        "Więcej wierszy i kolumn", "Wyjdź z pełnego ekranu", "Pełny ekran", "Terminal na całym ekranie",
        "Wyślij Ctrl+C", "Przerywa działający program", "Wyślij Ctrl+D",
        "Koniec danych - przy pustym wierszu poleceń kończy powłokę", "Wyślij Ctrl+Z",
        "Wstrzymuje działający program", "Zamknij terminal", "Kończy powłokę i wszystkie uruchomione z niej programy",
    ],
    "Deutsch": [
        "Terminal", "Die Shell kann nicht gestartet werden:", "Die Shell wurde beendet.",
        "Zum Schließen eine beliebige Taste drücken.", "Zeilen zurück:", "Schließen", "Tippen",
        "Tastatur ausblenden", "Tastatur einblenden", "Menü", "Shift/Ctrl", "Blättern", "Bild auf/ab",
        "Die Bildschirmtastatur für das Gamepad", "Größerer Text", "Weniger, größere Zeichen", "Kleinerer Text",
        "Mehr Zeilen und Spalten", "Vollbild verlassen", "Vollbild", "Das Terminal über den ganzen Bildschirm",
        "Ctrl+C senden", "Unterbricht das laufende Programm", "Ctrl+D senden",
        "Ende der Eingabe - bei leerer Eingabezeile wird die Shell beendet", "Ctrl+Z senden",
        "Hält das laufende Programm an", "Terminal schließen", "Beendet die Shell und alle daraus gestarteten Programme",
    ],
    "French": [
        "Terminal", "Impossible de lancer le shell :", "Le shell s'est terminé.", "Appuyez sur un bouton pour fermer.",
        "Lignes en arrière :", "Fermer", "Taper", "Masquer le clavier", "Afficher le clavier", "Menu", "Shift/Ctrl",
        "Défiler", "Page préc./suiv.", "Le clavier à l'écran pour la manette", "Texte plus grand",
        "Moins de caractères, plus grands", "Texte plus petit", "Plus de lignes et de colonnes",
        "Quitter le plein écran", "Plein écran", "Le terminal sur tout l'écran", "Envoyer Ctrl+C",
        "Interrompt le programme en cours", "Envoyer Ctrl+D", "Fin de saisie - à une invite vide, ferme le shell",
        "Envoyer Ctrl+Z", "Suspend le programme en cours", "Fermer le terminal",
        "Termine le shell et tous les programmes lancés depuis celui-ci",
    ],
    "Spanish": [
        "Terminal", "No se puede iniciar el shell:", "El shell ha terminado.", "Pulsa cualquier botón para cerrar.",
        "Líneas atrás:", "Cerrar", "Escribir", "Ocultar teclado", "Mostrar teclado", "Menú", "Shift/Ctrl",
        "Desplazar", "Re Pág/Av Pág", "El teclado en pantalla para el mando", "Texto más grande",
        "Menos caracteres, más grandes", "Texto más pequeño", "Más filas y columnas", "Salir de pantalla completa",
        "Pantalla completa", "El terminal en toda la pantalla", "Enviar Ctrl+C", "Interrumpe el programa en ejecución",
        "Enviar Ctrl+D", "Fin de la entrada - con la línea de órdenes vacía, cierra el shell", "Enviar Ctrl+Z",
        "Suspende el programa en ejecución", "Cerrar el terminal",
        "Termina el shell y todos los programas iniciados desde él",
    ],
    "Italiano": [
        "Terminale", "Impossibile avviare la shell:", "La shell è terminata.", "Premi un tasto qualsiasi per chiudere.",
        "Righe indietro:", "Chiudi", "Scrivi", "Nascondi tastiera", "Mostra tastiera", "Menu", "Shift/Ctrl", "Scorri",
        "Pagina su/giù", "La tastiera a schermo per il controller", "Testo più grande", "Meno caratteri, più grandi",
        "Testo più piccolo", "Più righe e colonne", "Esci da schermo intero", "Schermo intero",
        "Il terminale su tutto lo schermo", "Invia Ctrl+C", "Interrompe il programma in esecuzione", "Invia Ctrl+D",
        "Fine dell'input - con il prompt vuoto chiude la shell", "Invia Ctrl+Z", "Sospende il programma in esecuzione",
        "Chiudi il terminale", "Termina la shell e tutti i programmi avviati da essa",
    ],
    "Portuguese_BR": [
        "Terminal", "Não foi possível iniciar o shell:", "O shell foi encerrado.",
        "Pressione qualquer botão para fechar.", "Linhas acima:", "Fechar", "Digitar", "Ocultar teclado",
        "Mostrar teclado", "Menu", "Shift/Ctrl", "Rolar", "Page Up/Down", "O teclado na tela para o controle",
        "Texto maior", "Menos caracteres, maiores", "Texto menor", "Mais linhas e colunas", "Sair da tela cheia",
        "Tela cheia", "O terminal na tela inteira", "Enviar Ctrl+C", "Interrompe o programa em execução",
        "Enviar Ctrl+D", "Fim da entrada - com o prompt vazio, encerra o shell", "Enviar Ctrl+Z",
        "Suspende o programa em execução", "Fechar o terminal",
        "Encerra o shell e todos os programas iniciados a partir dele",
    ],
    "Dutch": [
        "Terminal", "Kan de shell niet starten:", "De shell is beëindigd.", "Druk op een knop om te sluiten.",
        "Regels terug:", "Sluiten", "Typen", "Toetsenbord verbergen", "Toetsenbord tonen", "Menu", "Shift/Ctrl",
        "Scrollen", "Page Up/Down", "Het schermtoetsenbord voor de controller", "Grotere tekst",
        "Minder, grotere tekens", "Kleinere tekst", "Meer rijen en kolommen", "Volledig scherm verlaten",
        "Volledig scherm", "De terminal over het hele scherm", "Ctrl+C versturen", "Onderbreekt het lopende programma",
        "Ctrl+D versturen", "Einde invoer - bij een lege prompt sluit het de shell", "Ctrl+Z versturen",
        "Pauzeert het lopende programma", "Terminal sluiten",
        "Beëindigt de shell en alle programma's die ervandaan zijn gestart",
    ],
    "Swedish": [
        "Terminal", "Det gick inte att starta skalet:", "Skalet har avslutats.",
        "Tryck på valfri knapp för att stänga.", "Rader bakåt:", "Stäng", "Skriv", "Dölj tangentbord",
        "Visa tangentbord", "Meny", "Shift/Ctrl", "Rulla", "Page Up/Down", "Skärmtangentbordet för handkontrollen",
        "Större text", "Färre, större tecken", "Mindre text", "Fler rader och kolumner", "Lämna helskärm", "Helskärm",
        "Terminalen över hela skärmen", "Skicka Ctrl+C", "Avbryter det program som körs", "Skicka Ctrl+D",
        "Slut på indata - vid en tom prompt avslutas skalet", "Skicka Ctrl+Z", "Pausar det program som körs",
        "Stäng terminalen", "Avslutar skalet och alla program som startats från det",
    ],
    "Danish": [
        "Terminal", "Kan ikke starte skallen:", "Skallen er afsluttet.", "Tryk på en vilkårlig knap for at lukke.",
        "Linjer tilbage:", "Luk", "Skriv", "Skjul tastatur", "Vis tastatur", "Menu", "Shift/Ctrl", "Rul",
        "Page Up/Down", "Skærmtastaturet til controlleren", "Større tekst", "Færre, større tegn", "Mindre tekst",
        "Flere rækker og kolonner", "Forlad fuld skærm", "Fuld skærm", "Terminalen over hele skærmen", "Send Ctrl+C",
        "Afbryder det kørende program", "Send Ctrl+D", "Slut på input - ved en tom prompt afsluttes skallen",
        "Send Ctrl+Z", "Sætter det kørende program på pause", "Luk terminalen",
        "Afslutter skallen og alle programmer startet fra den",
    ],
    "Finnish": [
        "Pääte", "Komentotulkkia ei voi käynnistää:", "Komentotulkki on päättynyt.",
        "Sulje painamalla mitä tahansa painiketta.", "Rivejä taaksepäin:", "Sulje", "Kirjoita", "Piilota näppäimistö",
        "Näytä näppäimistö", "Valikko", "Shift/Ctrl", "Vieritä", "Page Up/Down", "Näyttönäppäimistö ohjaimelle",
        "Suurempi teksti", "Vähemmän, suurempia merkkejä", "Pienempi teksti", "Enemmän rivejä ja sarakkeita",
        "Poistu koko näytöstä", "Koko näyttö", "Pääte koko näytöllä", "Lähetä Ctrl+C",
        "Keskeyttää käynnissä olevan ohjelman", "Lähetä Ctrl+D",
        "Syötteen loppu - tyhjällä kehotteella lopettaa komentotulkin", "Lähetä Ctrl+Z",
        "Pysäyttää käynnissä olevan ohjelman", "Sulje pääte", "Lopettaa komentotulkin ja kaikki siitä käynnistetyt ohjelmat",
    ],
    "Czech": [
        "Terminál", "Shell nelze spustit:", "Shell skončil.", "Stiskněte libovolné tlačítko pro zavření.",
        "Řádků zpět:", "Zavřít", "Psát", "Skrýt klávesnici", "Zobrazit klávesnici", "Nabídka", "Shift/Ctrl",
        "Posouvat", "Page Up/Down", "Klávesnice na obrazovce pro ovladač", "Větší text", "Méně, ale větších znaků",
        "Menší text", "Více řádků a sloupců", "Ukončit celou obrazovku", "Celá obrazovka",
        "Terminál přes celou obrazovku", "Odeslat Ctrl+C", "Přeruší běžící program", "Odeslat Ctrl+D",
        "Konec vstupu - na prázdném řádku ukončí shell", "Odeslat Ctrl+Z", "Pozastaví běžící program",
        "Zavřít terminál", "Ukončí shell a všechny programy z něj spuštěné",
    ],
    "Slovak": [
        "Terminál", "Shell nie je možné spustiť:", "Shell skončil.", "Stlačte ľubovoľné tlačidlo na zatvorenie.",
        "Riadkov späť:", "Zavrieť", "Písať", "Skryť klávesnicu", "Zobraziť klávesnicu", "Ponuka", "Shift/Ctrl",
        "Posúvať", "Page Up/Down", "Klávesnica na obrazovke pre ovládač", "Väčší text", "Menej, ale väčších znakov",
        "Menší text", "Viac riadkov a stĺpcov", "Ukončiť celú obrazovku", "Celá obrazovka",
        "Terminál cez celú obrazovku", "Odoslať Ctrl+C", "Preruší bežiaci program", "Odoslať Ctrl+D",
        "Koniec vstupu - na prázdnom riadku ukončí shell", "Odoslať Ctrl+Z", "Pozastaví bežiaci program",
        "Zavrieť terminál", "Ukončí shell a všetky programy z neho spustené",
    ],
    "Romanian": [
        "Terminal", "Shell-ul nu poate fi pornit:", "Shell-ul s-a încheiat.", "Apăsați orice buton pentru a închide.",
        "Linii înapoi:", "Închide", "Scrie", "Ascunde tastatura", "Arată tastatura", "Meniu", "Shift/Ctrl",
        "Derulare", "Page Up/Down", "Tastatura de pe ecran pentru controler", "Text mai mare",
        "Mai puține caractere, mai mari", "Text mai mic", "Mai multe rânduri și coloane",
        "Ieșire din ecran complet", "Ecran complet", "Terminalul pe tot ecranul", "Trimite Ctrl+C",
        "Întrerupe programul în curs", "Trimite Ctrl+D", "Sfârșitul intrării - la un prompt gol închide shell-ul",
        "Trimite Ctrl+Z", "Suspendă programul în curs", "Închide terminalul",
        "Închide shell-ul și toate programele pornite din el",
    ],
    "Turkish": [
        "Terminal", "Kabuk başlatılamıyor:", "Kabuk sonlandı.", "Kapatmak için herhangi bir düğmeye basın.",
        "Geri satır:", "Kapat", "Yaz", "Klavyeyi gizle", "Klavyeyi göster", "Menü", "Shift/Ctrl", "Kaydır",
        "Page Up/Down", "Oyun kolu için ekran klavyesi", "Daha büyük yazı", "Daha az, daha büyük karakter",
        "Daha küçük yazı", "Daha fazla satır ve sütun", "Tam ekrandan çık", "Tam ekran", "Terminal tüm ekranda",
        "Ctrl+C gönder", "Çalışan programı keser", "Ctrl+D gönder", "Girdi sonu - boş komut satırında kabuğu kapatır",
        "Ctrl+Z gönder", "Çalışan programı askıya alır", "Terminali kapat",
        "Kabuğu ve ondan başlatılan tüm programları sonlandırır",
    ],
    "Occitan": [
        "Terminal", "Impossible d'aviar lo shell :", "Lo shell s'es acabat.", "Quichatz un boton per tampar.",
        "Linhas en arrièr :", "Tampar", "Picar", "Amagar lo clavièr", "Mostrar lo clavièr", "Menú", "Shift/Ctrl",
        "Desfilar", "Page Up/Down", "Lo clavièr a l'ecran per la maneta", "Tèxte mai grand",
        "Mens de caractèrs, mai grands", "Tèxte mai pichon", "Mai de linhas e de colomnas", "Quitar l'ecran complet",
        "Ecran complet", "Lo terminal sus tot l'ecran", "Mandar Ctrl+C", "Interromp lo programa en cors",
        "Mandar Ctrl+D", "Fin de la picada - a una invita voida, tampa lo shell", "Mandar Ctrl+Z",
        "Suspend lo programa en cors", "Tampar lo terminal", "Acaba lo shell e totes los programas aviats a partir d'el",
    ],
    "Chinese_Simplified": [
        "终端", "无法启动 Shell：", "Shell 已结束。", "按任意按钮关闭。", "已回滚行数：", "关闭", "输入", "隐藏键盘",
        "显示键盘", "菜单", "Shift/Ctrl", "滚动", "上一页/下一页", "手柄使用的屏幕键盘", "放大文字", "字符更少、更大",
        "缩小文字", "更多行和列", "退出全屏", "全屏", "终端占满整个屏幕", "发送 Ctrl+C", "中断正在运行的程序",
        "发送 Ctrl+D", "输入结束 - 在空提示符下会退出 Shell", "发送 Ctrl+Z", "暂停正在运行的程序", "关闭终端",
        "结束 Shell 及其启动的所有程序",
    ],
}


def write(lang, values):
    path = os.path.join(os.path.dirname(__file__), "..", "resources", "lang", lang + ".txt")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# AutoBleem %s Translation - terminal\n" % lang)
        f.write("# Format: English Text=Translated Text (a text missing here falls through to the main GUI's file)\n")
        f.write("# Untranslated: 0 / Total: %d\n\n" % len(KEYS))
        for k, v in sorted(zip(KEYS, values), key=lambda kv: kv[0].lower()):
            assert "=" not in k and "=" not in v, (lang, k)
            f.write("%s=%s\n" % (k, v))


def main():
    write("English", KEYS)
    for lang, values in T.items():
        assert len(values) == len(KEYS), (lang, len(values))
        write(lang, values)
    print("%d languages, %d strings each" % (len(T) + 1, len(KEYS)))


if __name__ == "__main__":
    main()
