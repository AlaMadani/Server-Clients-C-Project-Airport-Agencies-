#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define MAX_BUFFER 5000
#define HISTO_FILE "histo.txt"
#define VOLS_FILE "vols.txt"
#define FACTURE_FILE "facture.txt"

pthread_mutex_t vols_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t facture_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t histo_mutex = PTHREAD_MUTEX_INITIALIZER;

void consulter_historique_vols(char *response)
{
    FILE *file = fopen(HISTO_FILE, "r");
    if (file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", HISTO_FILE);
        return;
    }
    snprintf(response, MAX_BUFFER, "\n=== Historique des Transactions ===\nRef Vol | Agence | Transaction | Valeur | Resultat\n------------------------------------------------------------\n");
    char ref_vol[20], agence[20], transaction[20], valeur[20], resultat[20];
    char line[200];
    while (fscanf(file, "%[^,],%[^,],%[^,],%[^,],%s\n", ref_vol, agence, transaction, valeur, resultat) == 5)
    {
        snprintf(line, sizeof(line), "%s | %s | %s | %s | %s\n", ref_vol, agence, transaction, valeur, resultat);
        strncat(response, line, MAX_BUFFER - strlen(response) - 1);
    }
    fclose(file);
}

void consulter_vols(char *response)
{
    FILE *file = fopen(VOLS_FILE, "r");
    if (file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", VOLS_FILE);
        return;
    }
    snprintf(response, MAX_BUFFER, "\n=== Liste des Vols ===\nRef Vol | Destination | Places Dispo | Prix Place\n--------------------------------------------\n");
    char ref_vol[20], destination[50];
    int nb_places, prix_place;
    char line[200];
    while (fscanf(file, "%[^,],%[^,],%d,%d\n", ref_vol, destination, &nb_places, &prix_place) == 4)
    {
        snprintf(line, sizeof(line), "%s | %s | %d | %d\n", ref_vol, destination, nb_places, prix_place);
        strncat(response, line, MAX_BUFFER - strlen(response) - 1);
    }
    fclose(file);
}

void consulter_toutes_factures(char *response)
{
    FILE *file = fopen(FACTURE_FILE, "r");
    if (file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", FACTURE_FILE);
        return;
    }
    snprintf(response, MAX_BUFFER, "\n=== Toutes les Factures ===\nRef Agence | Somme a Payer\n--------------------------\n");
    char ref_agence[20];
    int somme;
    char line[50];
    while (fscanf(file, "%[^,],%d\n", ref_agence, &somme) == 2)
    {
        snprintf(line, sizeof(line), "%s | %d\n", ref_agence, somme);
        strncat(response, line, MAX_BUFFER - strlen(response) - 1);
    }
    fclose(file);
}

void consulter_facture_agence(char *id_agence, char *response)
{
    FILE *file = fopen(FACTURE_FILE, "r");
    if (file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", FACTURE_FILE);
        return;
    }
    char ref_agence[20];
    int somme, found = 0;
    while (fscanf(file, "%[^,],%d\n", ref_agence, &somme) == 2)
    {
        if (strcmp(ref_agence, id_agence) == 0)
        {
            snprintf(response, MAX_BUFFER, "\n=== Facture de l'Agence %s ===\nSomme a payer : %d\n", id_agence, somme);
            found = 1;
            break;
        }
    }
    if (!found)
    {
        snprintf(response, MAX_BUFFER, "Aucune facture trouvee pour l'agence %s\n", id_agence);
    }
    fclose(file);
}

void traiter_transaction(char *id_agence, char *ref_vol, int nb_places, char *response)
{
    pthread_mutex_lock(&vols_mutex);
    pthread_mutex_lock(&facture_mutex);
    pthread_mutex_lock(&histo_mutex);

    FILE *vols_file = fopen(VOLS_FILE, "r");
    if (vols_file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", VOLS_FILE);
        goto unlock;
    }
    FILE *temp_file = fopen("temp.txt", "w");
    if (temp_file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir temp.txt\n");
        fclose(vols_file);
        goto unlock;
    }

    char v_ref[20], destination[50];
    int places, prix, found = 0, total_cost = 0;
    while (fscanf(vols_file, "%[^,],%[^,],%d,%d\n", v_ref, destination, &places, &prix) == 4)
    {
        if (strcmp(v_ref, ref_vol) == 0 && places >= nb_places)
        {
            found = 1;
            total_cost = nb_places * prix;
            fprintf(temp_file, "%s,%s,%d,%d\n", v_ref, destination, places - nb_places, prix);
        }
        else
        {
            fprintf(temp_file, "%s,%s,%d,%d\n", v_ref, destination, places, prix);
        }
    }
    fclose(vols_file);
    fclose(temp_file);
    if (!found)
    {
        snprintf(response, MAX_BUFFER, "REFUSE : Vol %s non trouve ou places insuffisantes\n", ref_vol);
        remove("temp.txt");
        goto unlock;
    }
    rename("temp.txt", VOLS_FILE);

    FILE *facture_file = fopen(FACTURE_FILE, "r");
    temp_file = fopen("temp.txt", "w");
    char f_agence[10];
    int somme, f_found = 0;
    while (fscanf(facture_file, "%[^,],%d\n", f_agence, &somme) == 2)
    {
        if (strcmp(f_agence, id_agence) == 0)
        {
            somme += total_cost;
            f_found = 1;
            fprintf(temp_file, "%s,%d\n", f_agence, somme);
        }
        else
        {
            fprintf(temp_file, "%s,%d\n", f_agence, somme);
        }
    }
    if (!f_found)
    {
        fprintf(temp_file, "%s,%d\n", id_agence, total_cost);
    }
    fclose(facture_file);
    fclose(temp_file);
    rename("temp.txt", FACTURE_FILE);

    FILE *histo_file = fopen(HISTO_FILE, "a");
    fprintf(histo_file, "%s,%s,RESERVATION,%d,ACCEPTE\n", ref_vol, id_agence, nb_places);
    fclose(histo_file);

    snprintf(response, MAX_BUFFER, "ACCEPTE : Reservation confirmee pour %d places sur vol %s. Facture : %d\n", nb_places, ref_vol, total_cost); // TODO a mdofier (ajouter Transaction/Valuer/Resultat)

unlock:
    pthread_mutex_unlock(&histo_mutex);
    pthread_mutex_unlock(&facture_mutex);
    pthread_mutex_unlock(&vols_mutex);
}

void annuler_reservation(char *id_agence, char *ref_vol, int nb_places, char *response)
{
    pthread_mutex_lock(&vols_mutex);
    pthread_mutex_lock(&facture_mutex);
    pthread_mutex_lock(&histo_mutex);

    int som;
    FILE *vols_file = fopen(VOLS_FILE, "r");
    if (vols_file == NULL)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Impossible d'ouvrir %s\n", VOLS_FILE);
        goto unlock;
    }
    FILE *temp_file = fopen("temp.txt", "w");
    char v_ref[20], destination[50];
    int places, prix, found = 0, penalty = 0;
    while (fscanf(vols_file, "%[^,],%[^,],%d,%d\n", v_ref, destination, &places, &prix) == 4)
    {
        if (strcmp(v_ref, ref_vol) == 0)
        {
            found = 1;
            penalty = (nb_places * prix * 10) / 100;
            som = nb_places * prix;
            fprintf(temp_file, "%s,%s,%d,%d\n", v_ref, destination, places + nb_places, prix);
        }
        else
        {
            fprintf(temp_file, "%s,%s,%d,%d\n", v_ref, destination, places, prix);
        }
    }
    fclose(vols_file);
    fclose(temp_file);
    if (!found)
    {
        snprintf(response, MAX_BUFFER, "Erreur : Vol %s non trouve\n", ref_vol);
        remove("temp.txt");
        goto unlock;
    }
    rename("temp.txt", VOLS_FILE);

    FILE *facture_file = fopen(FACTURE_FILE, "r");
    temp_file = fopen("temp.txt", "w");
    char f_agence[20];
    int somme, f_found = 0;
    while (fscanf(facture_file, "%[^,],%d\n", f_agence, &somme) == 2)
    {
        if (strcmp(f_agence, id_agence) == 0)
        {
            somme += penalty;
            somme -= som;

            f_found = 1;
            fprintf(temp_file, "%s,%d\n", f_agence, somme);
        }
        else
        {
            fprintf(temp_file, "%s,%d\n", f_agence, somme);
        }
    }
    fclose(facture_file);
    fclose(temp_file);
    rename("temp.txt", FACTURE_FILE);

    FILE *histo_file = fopen(HISTO_FILE, "a");
    fprintf(histo_file, "%s,%s,ANNULATION,%d,ACCEPTE\n", ref_vol, id_agence, nb_places);
    fclose(histo_file);

    snprintf(response, MAX_BUFFER, "ACCEPTE : Annulation de %d places sur vol %s. Penalite : %d\n", nb_places, ref_vol, penalty);

unlock:
    pthread_mutex_unlock(&histo_mutex);
    pthread_mutex_unlock(&facture_mutex);
    pthread_mutex_unlock(&vols_mutex);
}

void *handle_client(void *arg)
{
    int newsockfd = *(int *)arg;
    free(arg);
    char buffer[MAX_BUFFER];
    bzero(buffer, MAX_BUFFER);
    int n = read(newsockfd, buffer, MAX_BUFFER - 1);
    if (n < 0)
    {
        perror("Erreur de lecture");
        close(newsockfd);
        return NULL;
    }
    char response[MAX_BUFFER];
    bzero(response, MAX_BUFFER);

    if (strstr(buffer, "RESERVATION"))
    {
        char id_agence[20], ref_vol[20];
        int nb_places;
        sscanf(buffer, "RESERVATION %s %s %d", id_agence, ref_vol, &nb_places);
        traiter_transaction(id_agence, ref_vol, nb_places, response);
    }
    else if (strstr(buffer, "ANNULATION"))
    {
        char id_agence[20], ref_vol[20];
        int nb_places;
        sscanf(buffer, "ANNULATION %s %s %d", id_agence, ref_vol, &nb_places);
        annuler_reservation(id_agence, ref_vol, nb_places, response);
    }
    else if (strstr(buffer, "CONSULTER_VOLS"))
    {
        consulter_vols(response);
    }
    else if (strstr(buffer, "CONSULTER_FACTURE"))
    {
        char id_agence[20];
        sscanf(buffer, "CONSULTER_FACTURE %s", id_agence);
        consulter_facture_agence(id_agence, response);
    }
    else if (strstr(buffer, "CONSULTER_HISTORIQUE"))
    {
        consulter_historique_vols(response);
    }

    n = write(newsockfd, response, strlen(response));
    if (n < 0)
        perror("Erreur d'ecriture");
    close(newsockfd);
    return NULL;
}

void setup_socket(int *sockfd, struct sockaddr_in *addr, int port)
{
    *sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    bzero(addr, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;
    if (bind(*sockfd, (struct sockaddr *)addr, sizeof(*addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    listen(*sockfd, 10);
}
// fonctions du menue du serveur ------------------------
//  Fonction pour consulter l'historique des transactions (histo.txt) *
void consulter_historique_vols_menu()
{
    pthread_mutex_lock(&histo_mutex);
    FILE *file = fopen("histo.txt", "r");
    if (file == NULL)
    {
        printf("Erreur : Impossible d'ouvrir le fichier histo.txt\n");
        pthread_mutex_unlock(&histo_mutex);
        return;
    }

    printf("\n=== Historique des Transactions ===\n");
    printf("Ref Vol | Agence | Transaction | Valeur | Resultat\n");
    printf("-----------------------------------------\n");

    char ref_vol[10], agence[10], transaction[20], valeur[10], resultat[20];
    while (fscanf(file, "%[^,],%[^,],%[^,],%[^,],%s\n", ref_vol, agence, transaction, valeur, resultat) == 5)
    {
        printf("%s | %s | %s | %s | %s\n", ref_vol, agence, transaction, valeur, resultat);
    }

    fclose(file);
    pthread_mutex_unlock(&histo_mutex);
}

// Fonction pour consulter les vols disponibles (vols.txt) *
void consulter_vols_menu()
{
    pthread_mutex_lock(&vols_mutex);
    FILE *file = fopen("vols.txt", "r");
    if (file == NULL)
    {
        printf("Erreur : Impossible d'ouvrir le fichier vols.txt\n");
        pthread_mutex_unlock(&vols_mutex);
        return;
    }

    printf("\n=== Liste des Vols ===\n");
    printf("Ref Vol | Destination | Places Dispo | Prix Place\n");
    printf("--------------------------------------------\n");

    char ref_vol[10], destination[50];
    int nb_places, prix_place;

    while (fscanf(file, "%[^,],%[^,],%d,%d\n", ref_vol, destination, &nb_places, &prix_place) == 4)
    {
        printf("%s | %s | %d | %d\n", ref_vol, destination, nb_places, prix_place);
    }

    fclose(file);
    pthread_mutex_unlock(&vols_mutex);
}

// Fonction pour consulter toutes les factures (facture.txt)*
void consulter_toutes_factures_menu()
{
    pthread_mutex_lock(&facture_mutex);
    FILE *file = fopen("facture.txt", "r");
    if (file == NULL)
    {
        printf("Erreur : Impossible d'ouvrir le fichier facture.txt\n");
        pthread_mutex_unlock(&facture_mutex);
        return;
    }

    printf("\n=== Toutes les Factures ===\n");
    printf("Ref Agence | Somme a Payer\n");
    printf("--------------------------\n");

    char ref_agence[10];
    int somme;
    while (fscanf(file, "%[^,],%d\n", ref_agence, &somme) == 2)
    {
        printf("%s | %d\n", ref_agence, somme);
    }

    fclose(file);
    pthread_mutex_unlock(&facture_mutex);
}

// Fonction pour consulter la facture d'une agence spécifique (facture.txt) *
void consulter_facture_agence_menu()
{
    char id_agence[10];
    printf("Entrez l'ID de l'agence : ");
    scanf("%s", id_agence);

    pthread_mutex_lock(&facture_mutex);
    FILE *file = fopen("facture.txt", "r");
    if (file == NULL)
    {
        printf("Erreur : Impossible d'ouvrir le fichier facture.txt\n");
        pthread_mutex_unlock(&facture_mutex);
        return;
    }

    char ref_agence[10];
    int somme, found = 0;
    while (fscanf(file, "%[^,],%d\n", ref_agence, &somme) == 2)
    {
        if (strcmp(ref_agence, id_agence) == 0)
        {
            printf("\n=== Facture de l'Agence %s ===\n", id_agence);
            printf("Somme a payer : %d\n", somme);
            found = 1;
            break;
        }
    }

    if (!found)
    {
        printf("Aucune facture trouvee pour l'agence %s\n", id_agence);
    }

    fclose(file);
    pthread_mutex_unlock(&facture_mutex);
}
//---------------------------------------------------

// Function to run the menu in a separate thread
void *menu_thread(void *arg)
{
    int choix;
    do
    {
        printf("\n=== Menu Serveur (Compagnie Aerienne) ===\n");
        printf("1. Consulter l'historique des vols\n");
        printf("2. Consulter les vols\n");
        printf("3. Consulter toutes les factures\n");
        printf("4. Consulter la facture d'une agence\n");
        printf("0. Quitter\n");
        printf("Entrez votre choix : ");
        scanf("%d", &choix);

        switch (choix)
        {
        case 1:
            consulter_historique_vols_menu();
            break;
        case 2:
            consulter_vols_menu();
            break;
        case 3:
            consulter_toutes_factures_menu();
            break;
        case 4:
            consulter_facture_agence_menu();
            break;
        case 0:
            printf("Programme termine.\n");
            break;
        default:
            printf("Choix invalide. Veuillez entrer un numero entre 0 et 4.\n");
        }
    } while (choix != 0);
    return NULL;
}
//----------------------------------------------------
int main()
{
    int reservation_socket, cancel_socket, vols_socket, fact_socket, histo_socket;
    struct sockaddr_in reservation_addr, cancel_addr, vols_addr, fact_addr, histo_addr;

    // Set up all sockets
    setup_socket(&reservation_socket, &reservation_addr, 5000);
    setup_socket(&cancel_socket, &cancel_addr, 5001);
    setup_socket(&vols_socket, &vols_addr, 5002);
    setup_socket(&fact_socket, &fact_addr, 5003);
    setup_socket(&histo_socket, &histo_addr, 5004);

    // Set SO_REUSEADDR to avoid "Address already in use" on restart
    int opt = 1;
    setsockopt(reservation_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(cancel_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(vols_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fact_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(histo_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    printf("Serveur en ecoute...\n");
    // thread du menue ---------------------------------------------------
    //  Start menu thread
    pthread_t menu_tid;
    if (pthread_create(&menu_tid, NULL, menu_thread, NULL) != 0)
    {
        perror("Menu thread creation failed");
        exit(EXIT_FAILURE);
    }
    //----------------------------------------------------
    // File descriptor set for select
    fd_set read_fds;
    int max_fd;
    int sockets[] = {reservation_socket, cancel_socket, vols_socket, fact_socket, histo_socket};
    int num_sockets = 5;

    while (1)
    {
        // Initialize file descriptor set
        FD_ZERO(&read_fds);
        max_fd = -1;
        for (int i = 0; i < num_sockets; i++)
        {
            FD_SET(sockets[i], &read_fds);
            if (sockets[i] > max_fd)
                max_fd = sockets[i];
        }

        // Wait for activity on any socket
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            perror("Select failed");
            continue;
        }

        // Check each socket for incoming connections
        for (int i = 0; i < num_sockets; i++)
        {
            if (FD_ISSET(sockets[i], &read_fds))
            {
                struct sockaddr_in cli_addr;
                socklen_t clilen = sizeof(cli_addr);
                int *newsockfd = malloc(sizeof(int));
                if (newsockfd == NULL)
                {
                    perror("Malloc failed");
                    continue;
                }

                *newsockfd = accept(sockets[i], (struct sockaddr *)&cli_addr, &clilen);
                if (*newsockfd < 0)
                {
                    perror("Accept failed");
                    free(newsockfd);
                    continue;
                }

                // Create thread to handle client
                pthread_t thread;
                if (pthread_create(&thread, NULL, handle_client, newsockfd) != 0)
                {
                    perror("Thread creation failed");
                    close(*newsockfd);
                    free(newsockfd);
                }
                else
                {
                    pthread_detach(thread);
                }
            }
        }
    }

    // Cleanup (unreachable in this loop, but included for completeness)
    close(reservation_socket);
    close(cancel_socket);
    close(vols_socket);
    close(fact_socket);
    close(histo_socket);
    return 0;
}
