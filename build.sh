#!/usr/bin/env bash
# =============================================================================
#  build.sh - Compilation de HDL Batch Installer (Windows / Git Bash / MSYS2)
# -----------------------------------------------------------------------------
#  - S'execute dans l'environnement courant (bash).
#  - Peut INSTALLER toute la toolchain necessaire (MSYS2 : gcc + wxWidgets 3.2).
#  - TUI par defaut (menu clavier) ; sous-commandes non-interactives possibles.
#
#  Backends de compilation :
#    * g++ + wx-config (MSYS2)      -> installable, recommande ici
#    * Code::Blocks en ligne de cmd -> si deja installe (utilise ta config wx)
#
#  Usage :
#    ./build.sh                 # TUI interactive
#    ./build.sh install         # installe/complete la toolchain (MSYS2)
#    ./build.sh build [cible]   # compile (cible: 64|32 ; defaut 64)
#    ./build.sh run  [cible]    # compile puis copie dans Release/ et lance
#    ./build.sh doctor          # affiche l'environnement detecte
#    ./build.sh clean           # supprime les objets de compilation
# =============================================================================

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_PATH")" && pwd)"
SCRIPT_ABS="$SCRIPT_DIR/$(basename "$SCRIPT_PATH")"
REPO_ROOT="$SCRIPT_DIR"
SRC_DIR="$REPO_ROOT/HDL-Batch-installer-SRC"
CBP="$SRC_DIR/HDL-Batch-installer.cbp"
RELEASE_DIR="$REPO_ROOT/Release"
CONFIG_FILE="$REPO_ROOT/build.config"

# ---- couleurs ---------------------------------------------------------------
if [ -t 1 ]; then
  C_RST=$'\e[0m'; C_DIM=$'\e[2m'; C_R=$'\e[31m'; C_G=$'\e[32m'
  C_Y=$'\e[33m'; C_B=$'\e[36m'; C_M=$'\e[35m'; C_INV=$'\e[7m'
else
  C_RST=; C_DIM=; C_R=; C_G=; C_Y=; C_B=; C_M=; C_INV=
fi
say()  { printf '%s\n' "  $*"; }
info() { printf '%s\n' "  ${C_B}$*${C_RST}"; }
ok()   { printf '%s\n' "  ${C_G}$*${C_RST}"; }
warn() { printf '%s\n' "  ${C_Y}$*${C_RST}"; }
err()  { printf '%s\n' "  ${C_R}$*${C_RST}" >&2; }
have() { command -v "$1" >/dev/null 2>&1; }

# =============================================================================
#  Persistance de configuration (chemins detectes)
# =============================================================================
CFG_MSYS2_ROOT=""; CFG_CB_EXE=""
load_config() { [ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE" 2>/dev/null || true; }
save_config() {
  {
    echo "CFG_MSYS2_ROOT=\"$MSYS2_ROOT\""
    echo "CFG_CB_EXE=\"$CB_EXE\""
  } > "$CONFIG_FILE"
}

# =============================================================================
#  Detection de la toolchain -> renseigne les variables globales
# =============================================================================
GPP=""; GCC=""; WINDRES=""; WXCONFIG=""; CB_EXE=""; MSYS2_ROOT=""
MINGW_BIN=""; ARCH_BITS="64"; HDL_LIB=""

find_msys2_root() {
  local c
  for c in "$CFG_MSYS2_ROOT" "/c/msys64" "/d/msys64" "/c/tools/msys64" "$HOME/msys64"; do
    [ -n "$c" ] && [ -x "$c/usr/bin/pacman.exe" ] && { echo "$c"; return; }
  done
}

find_codeblocks() {
  local c
  if have codeblocks; then command -v codeblocks; return; fi
  for c in "$CFG_CB_EXE" \
           "/c/Program Files/CodeBlocks/codeblocks.exe" \
           "/c/Program Files (x86)/CodeBlocks/codeblocks.exe" \
           "/c/CodeBlocks/codeblocks.exe"; do
    [ -n "$c" ] && [ -x "$c" ] && { echo "$c"; return; }
  done
}

# Les scripts MSYS2 (wx-config...) ont prefix=/mingw64 code en dur : ils ne
# fonctionnent QUE dans un shell MSYS2, ou "/" = C:\msys64. Depuis Git Bash,
# "/mingw64" = C:\Program Files\Git\mingw64 -> chemins wx errones.
# On se relance donc dans le shell MSYS2 MINGW64 pour la partie compilation.
maybe_reexec_in_msys2() {
  [ -n "${HDLB_IN_MSYS2:-}" ] && return 0            # deja relance
  local root; root="$(find_msys2_root)"
  [ -z "$root" ] && return 0                          # pas de MSYS2 -> on continue (ex: Code::Blocks)
  local cur msys_win
  cur="$(cygpath -m / 2>/dev/null)"; cur="${cur%/}"
  msys_win="$(cygpath -m "$root" 2>/dev/null || echo "$root")"; msys_win="${msys_win%/}"
  # deja dans un shell dont la racine est MSYS2 ? rien a faire.
  [ -n "$cur" ] && [ "${cur,,}" = "${msys_win,,}" ] && return 0
  info "Passage dans l'environnement MSYS2 MINGW64 (pour des chemins /mingw64 corrects)..."
  # NB: les variables d'env ne survivent pas au login shell MSYS2 -> on passe un
  # argument sentinelle (fiable) pour bloquer toute re-execution en boucle.
  exec env MSYSTEM=MINGW64 CHERE_INVOKING=1 \
       "$root/usr/bin/bash.exe" -l "$SCRIPT_ABS" __reexec_in_msys2 "$@"
}

detect_toolchain() {
  MSYS2_ROOT="$(find_msys2_root)"
  # On PRIORISE le toolchain MINGW64 (runtime msvcrt) : c'est indispensable pour
  # se lier a external/libps2hdd.a et aux DLL de Release/ (compilees TDM-GCC/msvcrt).
  # Un shell UCRT64 produirait un mismatch de CRT a l'edition de liens.
  if [ -n "$MSYS2_ROOT" ]; then
    local sub mb
    for sub in mingw64 ucrt64 mingw32; do
      mb="$MSYS2_ROOT/$sub/bin"
      [ -x "$mb/g++.exe" ] || continue
      GPP="$mb/g++.exe"
      [ -x "$mb/gcc.exe" ]     && GCC="$mb/gcc.exe"
      [ -x "$mb/windres.exe" ] && WINDRES="$mb/windres.exe"
      [ -x "$mb/wx-config" ]   && WXCONFIG="$mb/wx-config"
      break
    done
  fi
  # a defaut, outils presents dans le PATH courant
  [ -z "$GPP" ]      && have g++       && GPP="$(command -v g++)"
  [ -z "$GCC" ]      && have gcc       && GCC="$(command -v gcc)"
  [ -z "$WINDRES" ]  && have windres   && WINDRES="$(command -v windres)"
  [ -z "$WXCONFIG" ] && have wx-config && WXCONFIG="$(command -v wx-config)"

  [ -n "$GPP" ] && MINGW_BIN="$(dirname "$GPP")"
  CB_EXE="$(find_codeblocks)"

  # architecture (32/64) d'apres le compilateur
  if [ -n "$GPP" ]; then
    case "$("$GPP" -dumpmachine 2>/dev/null)" in
      i686*|i386*) ARCH_BITS="32" ;;
      *)           ARCH_BITS="64" ;;
    esac
  fi
  if [ "$ARCH_BITS" = "32" ]; then
    HDL_LIB="$SRC_DIR/external/libps2hdd-x86.a"
  else
    HDL_LIB="$SRC_DIR/external/libps2hdd.a"
  fi
}

# wx-config peut etre un script shell : on l'appelle via bash pour etre sur.
wxc() { bash "$WXCONFIG" "$@"; }

doctor() {
  printf '\n  %s\n\n' "${C_M}== Environnement detecte ==${C_RST}"
  printf '    %-16s %s\n' "Shell MSYSTEM" "${MSYSTEM:-<Git Bash / inconnu>}"
  printf '    %-16s %s\n' "g++"        "${GPP:-${C_R}absent${C_RST}}"
  printf '    %-16s %s\n' "windres"    "${WINDRES:-${C_R}absent${C_RST}}"
  printf '    %-16s %s\n' "wx-config"  "${WXCONFIG:-${C_Y}absent${C_RST}}"
  if [ -n "$WXCONFIG" ]; then
    printf '    %-16s %s\n' "  wx version" "$(wxc --version 2>/dev/null)"
  fi
  printf '    %-16s %s\n' "MSYS2"      "${MSYS2_ROOT:-${C_Y}non installe${C_RST}}"
  printf '    %-16s %s\n' "Code::Blocks" "${CB_EXE:-${C_Y}absent${C_RST}}"
  printf '    %-16s %s\n' "Arch"       "${ARCH_BITS} bits"
  printf '    %-16s %s\n' "libps2hdd"  "$([ -f "$HDL_LIB" ] && echo "$HDL_LIB" || echo "${C_R}introuvable${C_RST}")"
  echo
  case "$GPP" in
    *ucrt64*) warn "Toolchain UCRT64 detectee : risque de mismatch CRT avec libps2hdd.a (msvcrt)." ;
              warn "Prefere le shell/paquets MINGW64. Lance ./build.sh install pour les poser." ;;
  esac
  if [ -n "$WXCONFIG" ] && [ -n "$GPP" ]; then
    ok  "Backend g++ pret."
  elif [ -n "$CB_EXE" ]; then
    warn "Backend g++ incomplet, mais Code::Blocks est disponible."
  else
    warn "Aucun backend pret. Lance : ./build.sh install"
  fi
}

# =============================================================================
#  Installation de la toolchain (MSYS2 + pacman)
# =============================================================================

# lance une commande pacman, soit directement (dans MSYS2), soit via le bash
# de l'installation MSYS2 detectee (depuis Git Bash).
pac() {
  if have pacman; then
    pacman "$@"
  elif [ -n "$MSYS2_ROOT" ]; then
    MSYSTEM=MSYS "$MSYS2_ROOT/usr/bin/bash.exe" -lc "pacman $*"
  else
    return 127
  fi
}

install_msys2_itself() {
  info "MSYS2 n'est pas installe. Tentative d'installation automatique..."
  if have winget; then
    say "winget install MSYS2.MSYS2 ..."
    winget install --id MSYS2.MSYS2 -e --accept-package-agreements --accept-source-agreements || true
  elif have choco; then
    say "choco install msys2 ..."
    choco install msys2 -y || true
  else
    err "Ni winget ni choco disponibles pour installer MSYS2 automatiquement."
    err "Installe MSYS2 manuellement depuis https://www.msys2.org puis relance :"
    err "    ./build.sh install"
    return 1
  fi
  MSYS2_ROOT="$(find_msys2_root)"
  if [ -z "$MSYS2_ROOT" ]; then
    err "MSYS2 introuvable apres installation. Redemarre le terminal et relance ./build.sh install"
    return 1
  fi
  ok "MSYS2 installe dans : $MSYS2_ROOT"
}

install_deps() {
  detect_toolchain
  if [ -z "$MSYS2_ROOT" ] && ! have pacman; then
    install_msys2_itself || return 1
  fi

  # On force la variante MINGW64 (msvcrt), PAS ucrt : requis pour se lier a
  # external/libps2hdd.a (compilee avec TDM-GCC / msvcrt). Ne PAS suivre
  # MINGW_PACKAGE_PREFIX s'il vaut ...ucrt...
  local P="mingw-w64-x86_64"
  [ "$ARCH_BITS" = "32" ] && P="mingw-w64-i686"
  info "Mise a jour de la base de paquets MSYS2..."
  pac -Sy --noconfirm || warn "pacman -Sy a renvoye une erreur (on continue)."

  info "Installation du compilateur (gcc, binutils/windres)..."
  pac -S --needed --noconfirm "${P}-gcc" "${P}-pkgconf" \
    || { err "Echec installation gcc."; return 1; }

  info "Installation de wxWidgets 3.2..."
  local wx_ok=1 pkg
  for pkg in "${P}-wxwidgets3.2-msw" "${P}-wxwidgets3.2" "${P}-wxWidgets"; do
    if pac -S --needed --noconfirm "$pkg"; then wx_ok=0; break; fi
  done
  if [ "$wx_ok" -ne 0 ]; then
    err "Impossible d'installer wxWidgets via pacman."
    err "Cherche le nom exact avec :  pacman -Ss wxwidgets"
    return 1
  fi

  detect_toolchain
  save_config
  echo
  if [ -n "$WXCONFIG" ] && [ -n "$GPP" ]; then
    ok "Toolchain prete."
    warn "Si wx-config reste 'absent' ici, ouvre le raccourci ${C_M}MSYS2 MINGW64${C_Y} et relance ./build.sh depuis la."
  else
    warn "Installation faite, mais les outils ne sont pas dans ce shell."
    warn "Ouvre ${C_M}MSYS2 MINGW64${C_Y} (menu Demarrer) et relance : ./build.sh"
  fi
}

# =============================================================================
#  Backend 1 : compilation g++ + wx-config
# =============================================================================
target_exe() {
  case "$1" in
    32) echo "$SRC_DIR/_bin/playground/HDL-Batch-installer-x86.exe" ;;
    *)  echo "$SRC_DIR/_bin/playground/HDL-Batch-installer.exe" ;;
  esac
}

build_gpp() {
  local bits="$1" clean="$2"
  [ -z "$WXCONFIG" ] && { err "wx-config introuvable. Lance : ./build.sh install"; return 1; }
  [ -z "$GPP" ]      && { err "g++ introuvable. Lance : ./build.sh install"; return 1; }
  [ -f "$HDL_LIB" ]  || { err "libps2hdd introuvable : $HDL_LIB"; return 1; }
  [ -z "$WINDRES" ] && WINDRES="windres"

  local obj_dir="$SRC_DIR/_obj/sh-$bits"
  local exe; exe="$(target_exe "$bits")"
  [ "$clean" = "1" ] && rm -rf "$obj_dir"
  mkdir -p "$obj_dir" "$(dirname "$exe")"

  local march wxcpu
  if [ "$bits" = "32" ]; then march="-m32"; wxcpu="-DWX_CPU_X86"; else march="-m64"; wxcpu="-DWX_CPU_AMD64"; fi

  local CXXFLAGS LIBS
  CXXFLAGS="$(wxc --cxxflags)" || { err "wx-config --cxxflags a echoue."; return 1; }
  LIBS="$(wxc --libs)"        || { err "wx-config --libs a echoue.";    return 1; }

  local proj_inc="-I$SRC_DIR -I$SRC_DIR/include -I$REPO_ROOT/include -I$SRC_DIR/xpm"
  # NB: on ne definit PAS UNICODE/_UNICODE (comme le projet d'origine) : sinon
  # windows.h renomme GetClassInfo->GetClassInfoW et casse les macros RTTI de wx.
  local defines="-D__GNUWIN32__ -DBITS=$bits -DHAVE_W32API_H"

  ( cd "$SRC_DIR" || exit 1

    # 1) ressources Windows (icone / manifeste admin / version / splash)
    info "[1/3] windres resource.rc"
    local resinc
    resinc="$(printf '%s\n' $CXXFLAGS | grep '^-I' | sed 's/^-I/--include-dir=/' | tr '\n' ' ')"
    "$WINDRES" $wxcpu $resinc --include-dir="$SRC_DIR" \
      -J rc -O coff -i resource.rc -o "$obj_dir/resource.o" \
      || { err "windres a echoue."; exit 1; }

    # 2) compilation
    local srcs=( *.cpp gamename/*.cpp ) objs=( "$obj_dir/resource.o" )
    local total="${#srcs[@]}" n=0 s obj
    for s in "${srcs[@]}"; do
      n=$((n+1))
      obj="$obj_dir/$(echo "$s" | tr '/\\' '__' | sed 's/\.cpp$/.o/')"
      # gamename/database.cpp est un fichier GENERE : un std::map de ~16k paires
      # <string,string> initialise par accolades. Cette init dynamique geante fait
      # planter cc1plus (GCC 16), meme en -O0 et avec 40 Go libres. On transforme
      # a la volee la donnee en tableau statique de const char* (donnee pure, aucun
      # constructeur a la compilation), rempli dans la map au runtime : strictement
      # equivalent, mais compile en ~0,5 s. Le fichier du depot n'est pas modifie.
      local src_to_compile="$s"
      if [ "$s" = "gamename/database.cpp" ]; then
        src_to_compile="$obj_dir/_database_arr.cpp"
        {
          echo '#include "database.h"'
          echo 'static const char* const GAMEDB_RAW[][2] = {'
          tail -n +3 "$s"                       # entrees {"k","v"}, ... suivies de };
          echo 'std::map<std::string,std::string> GameNameDB = []{'
          echo '    std::map<std::string,std::string> m;'
          echo '    for (auto& e : GAMEDB_RAW) m.emplace(e[0], e[1]);'
          echo '    return m;'
          echo '}();'
        } > "$src_to_compile"
        printf '  %s        (map ~16k entrees : transformee en tableau statique)%s\n' "$C_DIM" "$C_RST"
      fi
      printf '  %s[2/3] (%2d/%d) g++ -c %s%s\n' "$C_DIM" "$n" "$total" "$s" "$C_RST"
      "$GPP" -c "$src_to_compile" -o "$obj" $march -pipe -mthreads -std=gnu++17 -O2 \
        $defines $proj_inc -I"$SRC_DIR/gamename" $CXXFLAGS \
        || { err "Echec de compilation : $s"; exit 1; }
      objs+=( "$obj" )
    done

    # 3) edition de liens
    info "[3/3] link -> $(basename "$exe")"
    # -mwindows : sous-systeme GUI (pas de fenetre console detachee ; les logs
    # sont captures et affiches dans le panneau embarque en bas de la fenetre).
    "$GPP" "${objs[@]}" "$HDL_LIB" $LIBS \
      -lwininet -lrpcrt4 -lwinmm \
      -static-libgcc -static-libstdc++ $march -mthreads -mwindows \
      -o "$exe" \
      || { err "Echec de l'edition de liens."; exit 1; }
  ) || return 1

  [ -f "$exe" ]
}

# =============================================================================
#  Backend 2 : Code::Blocks en ligne de commande
# =============================================================================
build_codeblocks() {
  local cbtarget="$1" clean="$2"
  [ -z "$CB_EXE" ] && { err "Code::Blocks introuvable."; return 1; }
  local exe; exe="$(target_exe "$([ "$cbtarget" = "32-bits" ] && echo 32 || echo 64)")"
  local before=0; [ -f "$exe" ] && before="$(stat -c %Y "$exe" 2>/dev/null || echo 0)"
  local action="--build"; [ "$clean" = "1" ] && action="--rebuild"

  info "Code::Blocks : $action --target=$cbtarget"
  "$CB_EXE" --no-splash-screen "$action" "--target=$cbtarget" "$CBP" || true
  sleep 1
  local after=0; [ -f "$exe" ] && after="$(stat -c %Y "$exe" 2>/dev/null || echo 0)"
  [ "$after" -gt "$before" ]
}

# =============================================================================
#  Copie dans Release/ + lancement (pour tester)
# =============================================================================
deploy_run() {
  local exe="$1" do_run="$2"
  [ -d "$RELEASE_DIR" ] || { warn "Dossier Release/ absent : pas de lancement."; return; }
  cp -f "$exe" "$RELEASE_DIR/HDL-Batch-installer.exe"
  ok "Exe copie -> Release/HDL-Batch-installer.exe"
  # L'exe est lie a wxWidgets de MSYS2 (DLL *_gcc_custom.dll) et non aux DLL TDM
  # livrees dans Release/. On copie TOUTE la fermeture de dependances DLL (wx +
  # pcre2/zlib/png/tiff/stdc++...) par resolution recursive, pour que l'exe tourne
  # de maniere autonome depuis Release/ (sans /mingw64/bin dans le PATH).
  if [ -n "$MINGW_BIN" ]; then
    local OBJDUMP="$MINGW_BIN/objdump.exe"
    have objdump && OBJDUMP="$(command -v objdump)"
    local -A _seen
    local queue=("$RELEASE_DIR/HDL-Batch-installer.exe") cur dep n=0
    while [ ${#queue[@]} -gt 0 ]; do
      cur="${queue[0]}"; queue=("${queue[@]:1}")
      for dep in $("$OBJDUMP" -p "$cur" 2>/dev/null | grep 'DLL Name:' | sed 's/.*DLL Name: //' | tr -d '\r'); do
        if [ -f "$MINGW_BIN/$dep" ] && [ -z "${_seen[$dep]:-}" ]; then
          _seen[$dep]=1
          cp -f "$MINGW_BIN/$dep" "$RELEASE_DIR/"
          queue+=("$MINGW_BIN/$dep")
          n=$((n+1))
        fi
      done
    done
    say "  + $n DLL runtime copiees dans Release/"
  fi
  if [ "$do_run" = "1" ]; then
    info "Lancement avec elevation (le manifeste de l'app exige les droits admin -> UAC)..."
    local relwin; relwin="$(cygpath -w "$RELEASE_DIR")"
    powershell -NoProfile -Command \
      "Start-Process -FilePath '$relwin\\HDL-Batch-installer.exe' -WorkingDirectory '$relwin' -ArgumentList '--force-max-debug','--skip-update' -Verb RunAs" \
      || warn "Lancement refuse/annule (UAC)."
  fi
}

# =============================================================================
#  Compilation (dispatch backend) + resume
# =============================================================================
do_build() {
  local bits="$1" clean="$2" run="$3" backend="$4"
  local exe; exe="$(target_exe "$bits")"
  local start; start="$(date +%s)"

  info "=== Compilation ($bits bits / $backend) ==="
  local ok_build=1
  if [ "$backend" = "codeblocks" ]; then
    build_codeblocks "$([ "$bits" = "32" ] && echo 32-bits || echo 64-bits)" "$clean" && ok_build=0
  else
    build_gpp "$bits" "$clean" && ok_build=0
  fi

  local dur=$(( $(date +%s) - start ))
  echo
  if [ "$ok_build" -eq 0 ]; then
    ok "BUILD OK en ${dur}s"
    ok "-> $exe"
    [ "$run" = "1" ] && deploy_run "$exe" 1
    return 0
  else
    err "BUILD ECHOUE."
    [ "$backend" = "codeblocks" ] && warn "Ouvre le .cbp dans Code::Blocks pour le log detaille."
    return 1
  fi
}

# =============================================================================
#  TUI : menu clavier (fleches + Entree, q pour quitter)
# =============================================================================
MENU_RESULT=-1
menu() {
  local title="$1"; shift
  local options=( "$@" ) sel=0 key key2
  while true; do
    clear
    printf '\n  %s\n\n' "${C_M}== HDL Batch Installer -- Compilation ==${C_RST}"
    printf '  %s\n\n' "${C_B}$title${C_RST}"
    local i
    for i in "${!options[@]}"; do
      if [ "$i" -eq "$sel" ]; then
        printf '   %s > %s %s\n' "$C_INV" "${options[$i]}" "$C_RST"
      else
        printf '     %s\n' "${options[$i]}"
      fi
    done
    printf '\n  %s\n' "${C_DIM}Fleches + Entree, q pour quitter${C_RST}"
    IFS= read -rsn1 key
    case "$key" in
      $'\x1b') read -rsn2 -t 0.05 key2
               case "$key2" in
                 '[A') sel=$(( (sel - 1 + ${#options[@]}) % ${#options[@]} )) ;;
                 '[B') sel=$(( (sel + 1) % ${#options[@]} )) ;;
               esac ;;
      '')      MENU_RESULT="$sel"; return 0 ;;
      q|Q)     MENU_RESULT=-1; return 1 ;;
    esac
  done
}

confirm() { # $1 question ; retourne 0 = oui
  local a
  printf '  %s [O/n] ' "${C_Y}$1${C_RST}"
  read -r a
  case "${a,,}" in n|non|no) return 1 ;; *) return 0 ;; esac
}

tui() {
  # ecran d'accueil = doctor
  clear; doctor
  printf '\n  %s' "${C_DIM}Entree pour continuer...${C_RST}"; read -r _

  # si rien n'est pret, proposer l'installation
  if [ -z "$WXCONFIG" ] && [ -z "$CB_EXE" ]; then
    if confirm "Aucune toolchain prete. Installer le necessaire (MSYS2 + gcc + wxWidgets) ?"; then
      install_deps
      printf '\n  %s' "${C_DIM}Entree pour continuer...${C_RST}"; read -r _
    else
      return
    fi
  fi

  # menu principal
  while true; do
    menu "Menu principal" \
      "Compiler (64 bits)" \
      "Compiler et LANCER (64 bits)" \
      "Recompiler tout (clean, 64 bits)" \
      "Compiler en 32 bits" \
      "Installer / completer la toolchain" \
      "Diagnostic (doctor)" \
      "Quitter" || return
    case "$MENU_RESULT" in
      0) choose_backend && do_build 64 0 0 "$BK"; pause_return ;;
      1) choose_backend && do_build 64 0 1 "$BK"; pause_return ;;
      2) choose_backend && do_build 64 1 0 "$BK"; pause_return ;;
      3) choose_backend && do_build 32 0 0 "$BK"; pause_return ;;
      4) install_deps; pause_return ;;
      5) clear; doctor; pause_return ;;
      6|-1) return ;;
    esac
  done
}

BK="gpp"
choose_backend() {
  # choisit le backend ; si les deux dispo -> menu
  if [ -n "$WXCONFIG" ] && [ -n "$CB_EXE" ]; then
    menu "Methode de compilation" \
      "g++ + wx-config (MSYS2)" \
      "Code::Blocks (utilise ta config wx)" || return 1
    [ "$MENU_RESULT" = "1" ] && BK="codeblocks" || BK="gpp"
  elif [ -n "$WXCONFIG" ]; then BK="gpp"
  elif [ -n "$CB_EXE" ];   then BK="codeblocks"
  else err "Aucun backend disponible. Lance l'installation."; return 1; fi
  return 0
}
pause_return() { printf '\n  %s' "${C_DIM}Entree pour revenir au menu...${C_RST}"; read -r _; }

# =============================================================================
#  Point d'entree
# =============================================================================
load_config
# sentinelle posee par le re-exec dans MSYS2 (voir maybe_reexec_in_msys2)
if [ "${1:-}" = "__reexec_in_msys2" ]; then HDLB_IN_MSYS2=1; shift; fi
cmd="${1:-}"
# Pour tout ce qui compile/installe, on doit etre dans le shell MSYS2.
case "$cmd" in
  ""|menu|tui|install|build|run|rebuild|doctor) maybe_reexec_in_msys2 "$@" ;;
esac
detect_toolchain

case "$cmd" in
  ""|menu|tui)  tui ;;
  doctor)       doctor ;;
  install)      install_deps ;;
  clean)        rm -rf "$SRC_DIR/_obj"/sh-* && ok "Objets supprimes." ;;
  build)
      bits="${2:-64}"; [ "$bits" = "32-bits" ] && bits=32; [ "$bits" = "64-bits" ] && bits=64
      choose_backend && do_build "$bits" 0 0 "$BK" ;;
  run)
      bits="${2:-64}"; [ "$bits" = "32-bits" ] && bits=32; [ "$bits" = "64-bits" ] && bits=64
      choose_backend && do_build "$bits" 0 1 "$BK" ;;
  rebuild)
      bits="${2:-64}"; [ "$bits" = "32-bits" ] && bits=32; [ "$bits" = "64-bits" ] && bits=64
      choose_backend && do_build "$bits" 1 0 "$BK" ;;
  -h|--help|help)
      grep -E '^#( |=|$)' "$SCRIPT_PATH" | sed 's/^# \{0,1\}//' ;;
  *)  err "Commande inconnue : $cmd"; err "Voir : ./build.sh --help"; exit 1 ;;
esac
